# export_blueprints.py — UE 5.6.1 (carga robusta + normalización a UBlueprint*)
# Genera:
#   Saved/BlueprintExports/blueprints.json   (todo el conjunto)
#   Saved/BlueprintExports/blueprints.ndjson (1 línea por BP)
#
# Cobertura:
# - Blueprint (UBlueprint)
# - WidgetBlueprint (UWidgetBlueprint)
# - AnimBlueprint (UAnimBlueprint)
# - ControlRigBlueprint (UControlRigBlueprint) si el plugin está activo
#
# Para cada graph: nodes, pins, linked_to y connections (output→input, is_exec)

import json, hashlib, time
from pathlib import Path
import unreal

# ---------- CONFIG ----------
OUTPUT_DIR = Path(unreal.Paths.project_saved_dir()) / "BlueprintExports"
OUTPUT_JSON = OUTPUT_DIR / "blueprints.json"
OUTPUT_NDJSON = OUTPUT_DIR / "blueprints.ndjson"
VERBOSE_LOG = True
# ----------------------------

def now_iso():
    return time.strftime("%Y-%m-%dT%H:%M:%S")

def get_project_name_safe():
    try:
        return unreal.SystemLibrary.get_game_name()
    except Exception:
        pass
    try:
        proj_path = unreal.Paths.get_project_file_path()
        if proj_path:
            return Path(proj_path).stem
    except Exception:
        pass
    try:
        proj_dir = unreal.Paths.project_dir()
        if proj_dir:
            return Path(proj_dir).resolve().name
    except Exception:
        pass
    return "UnknownProject"

def make_hash(obj) -> str:
    s = json.dumps(obj, sort_keys=True, ensure_ascii=False)
    return hashlib.md5(s.encode("utf-8")).hexdigest()

def type_to_str(pin_type: unreal.EdGraphPinType) -> str:
    cat = pin_type.pin_category or ""
    sub = pin_type.pin_sub_category or ""
    obj = pin_type.pin_sub_category_object
    obj_name = obj.get_name() if obj else ""
    return "::".join([p for p in [cat, sub, obj_name] if p])

# ---------- CARGA ROBUSTA DE BLUEPRINT-LIKE ----------

def _load_asset_by_ad(a):
    """Devuelve (obj, path_str) cargando por ruta paquete+nombre usando EditorAssetLibrary."""
    try:
        path = f"{a.package_name}.{a.asset_name}"
    except Exception:
        # último recurso: intentar con a.object_path si existiera
        try:
            path = str(a.object_path)
        except Exception:
            return None, "<unknown>"
    obj = None
    try:
        obj = unreal.EditorAssetLibrary.load_asset(path)
    except Exception:
        obj = None
    return obj, path

def _normalize_to_blueprint(obj):
    """
    Intenta devolver un objeto 'Blueprint-like' edit-time:
      - UBlueprint
      - UWidgetBlueprint
      - UAnimBlueprint
      - UControlRigBlueprint (si está disponible)
    Si recibe BlueprintGeneratedClass, toma ClassGeneratedBy.
    """
    if obj is None:
        return None, "None"

    cls_name = obj.get_class().get_name() if obj.get_class() else str(type(obj))
    # Si es BlueprintGeneratedClass → volver al UBlueprint que lo generó
    try:
        if isinstance(obj, unreal.BlueprintGeneratedClass):
            gen_by = obj.get_editor_property("ClassGeneratedBy")
            if gen_by:
                return gen_by, f"{cls_name}→UBlueprint(ClassGeneratedBy)"
    except Exception:
        pass

    # UBlueprint estándar
    if isinstance(obj, unreal.Blueprint):
        return obj, "UBlueprint"

    # WidgetBlueprint
    try:
        if isinstance(obj, unreal.WidgetBlueprint):
            return obj, "UWidgetBlueprint"
    except Exception:
        pass

    # AnimBlueprint
    try:
        if isinstance(obj, unreal.AnimBlueprint):
            return obj, "UAnimBlueprint"
    except Exception:
        pass

    # ControlRigBlueprint (plugin ControlRig)
    try:
        CRB = getattr(unreal, "ControlRigBlueprint", None)
        if CRB and isinstance(obj, CRB):
            return obj, "UControlRigBlueprint"
    except Exception:
        pass

    # A veces EditorAssetLibrary.load_asset devuelve el 'UBlueprint' ya correcto pero con nombre de clase derivada
    # chequeo por nombre de clase
    lname = cls_name.lower()
    if "blueprint" in lname:
        return obj, cls_name

    return None, cls_name  # no es blueprint-like

# ---------- GRAFOS Y NODOS ----------

def _get_editor_graph_nodes(graph):
    """Devuelve lista de UEdGraphNode con fallbacks."""
    # 1) método usual
    try:
        nodes = graph.get_nodes()
        if nodes:
            return list(nodes)
    except Exception:
        pass
    # 2) propiedad
    try:
        nodes = getattr(graph, "nodes", None)
        if nodes:
            return list(nodes)
    except Exception:
        pass
    # 3) GraphEditorSubsystem
    try:
        ges = unreal.get_editor_subsystem(unreal.GraphEditorSubsystem)
        if ges is not None:
            nodes = ges.get_all_nodes(graph)
            if nodes:
                return list(nodes)
    except Exception:
        pass
    return []

def _get_all_graphs(bp):
    """Devuelve todos los UEdGraph del blueprint con deduplicación."""
    graphs = []

    # BlueprintEditorLibrary.get_all_graphs si existe
    try:
        bel = unreal.BlueprintEditorLibrary
        if hasattr(bel, "get_all_graphs"):
            gs = bel.get_all_graphs(bp)
            if gs:
                graphs.extend(gs)
    except Exception:
        pass

    # Campos comunes
    for attr in ("ubergraph_pages", "function_graphs", "macro_graphs"):
        try:
            arr = getattr(bp, attr, None)
            if arr:
                graphs.extend(arr)
        except Exception:
            pass

    # EventGraph directo
    try:
        eg = getattr(bp, "event_graph", None)
        if eg:
            graphs.append(eg)
    except Exception:
        pass

    # genérico: propiedad "graphs" si existe
    try:
        generic = getattr(bp, "graphs", None) or bp.get_editor_property("graphs")
        if generic:
            graphs.extend(list(generic))
    except Exception:
        pass

    # Deduplicación por outer.name + graph.name
    seen = set()
    out = []
    for g in graphs:
        try:
            outer_name = g.get_outer().get_name() if g.get_outer() else ""
            key = f"{outer_name}.{g.get_name()}"
        except Exception:
            key = g.get_name() if hasattr(g, "get_name") else str(id(g))
        if key not in seen:
            seen.add(key)
            out.append(g)
    return out

# ---------- EXPORT ----------

def export_graph(graph: unreal.EdGraph) -> dict:
    data = {
        "name": graph.get_name(),
        "schema": graph.schema_class().get_name() if graph.schema_class() else "",
        "nodes": [],
        "connections": []
    }

    nodes = _get_editor_graph_nodes(graph)
    seen_edges = set()

    for n in nodes:
        # título
        try:
            title = n.get_node_title(unreal.NodeTitleType.FULL_TITLE)
        except Exception:
            try:
                title = n.get_node_title(unreal.NodeTitleType.LIST_VIEW)
            except Exception:
                title = n.get_name()

        # pines
        pins = []
        try:
            for p in n.pins:
                linked = []
                try:
                    for l in p.linked_to:
                        linked.append({"node": l.get_owning_node().get_name(), "pin": l.get_name()})
                except Exception:
                    pass

                pins.append({
                    "name": p.get_name(),
                    "direction": "input" if p.direction == unreal.EEdGraphPinDirection.EGPD_Input else "output",
                    "type": (p.pin_type.pin_category or ""),
                    "default_value": getattr(p, "default_value", None),
                    "is_reference": p.pin_type.is_reference,
                    "is_const": p.pin_type.is_const,
                    "linked_to": linked
                })
        except Exception:
            pass

        pos_x = getattr(n, "node_pos_x", 0)
        pos_y = getattr(n, "node_pos_y", 0)

        data["nodes"].append({
            "id": n.get_name(),
            "title": str(title),
            "class": n.get_class().get_name() if n.get_class() else "",
            "pos": [pos_x, pos_y],
            "pins": pins
        })

    # edges output→input
    for n in nodes:
        for p in getattr(n, "pins", []):
            try:
                p_dir = "input" if p.direction == unreal.EEdGraphPinDirection.EGPD_Input else "output"
                for l in p.linked_to:
                    l_dir = "input" if l.direction == unreal.EEdGraphPinDirection.EGPD_Input else "output"

                    from_node, from_pin = n.get_name(), p.get_name()
                    to_node, to_pin     = l.get_owning_node().get_name(), l.get_name()

                    if p_dir == "input" and l_dir == "output":
                        from_node, from_pin, to_node, to_pin = to_node, to_pin, from_node, from_pin

                    key = f"{from_node}|{from_pin}->{to_node}|{to_pin}"
                    if key in seen_edges:
                        continue
                    seen_edges.add(key)

                    is_exec = False
                    try:
                        from_is_original_p = not (p_dir == "input" and l_dir == "output")
                        pin_ref = p if from_is_original_p else l
                        is_exec = ((pin_ref.pin_type.pin_category or "").lower() == "exec")
                    except Exception:
                        pass

                    data["connections"].append({
                        "from": {"node": from_node, "pin": from_pin},
                        "to":   {"node": to_node,   "pin": to_pin},
                        "is_exec": is_exec
                    })
            except Exception:
                pass

    return data

def _infer_function_and_macro_names(bp, graphs):
    functions, macros = [], []
    try:
        functions = [g.get_name() for g in getattr(bp, "function_graphs", []) or []]
    except Exception:
        pass
    try:
        macros = [g.get_name() for g in getattr(bp, "macro_graphs", []) or []]
    except Exception:
        pass
    if not functions or not macros:
        for g in graphs:
            try:
                gname = g.get_name()
                lname = (gname or "").lower()
                if "macro" in lname and gname not in macros:
                    macros.append(gname)
                elif "function" in lname and gname not in functions:
                    functions.append(gname)
            except Exception:
                pass
    return functions, macros

def export_blueprint(bp, kind_label="UBlueprint") -> dict:
    asset_name = bp.get_name()
    asset_path = bp.get_path_name()
    generated_class_path = bp.GeneratedClass.get_path_name() if getattr(bp, "GeneratedClass", None) else ""

    # variables
    variables = []
    try:
        for v in bp.new_variables:
            variables.append({
                "name": v.var_name,
                "type": type_to_str(v.var_type),
            })
    except Exception:
        pass

    # graphs
    g_objs = _get_all_graphs(bp)
    graphs = []
    total_nodes = 0
    for g in g_objs:
        try:
            g_doc = export_graph(g)
            graphs.append(g_doc)
            total_nodes += len(g_doc.get("nodes", []))
        except Exception as e:
            unreal.log_warning(f"[Exporter] No se pudo exportar un graph de {asset_name}: {e}")

    functions, macros = _infer_function_and_macro_names(bp, g_objs)

    # compilación si existe API
    compile_status = "Skipped"
    messages = []
    try:
        if hasattr(unreal.BlueprintEditorLibrary, "compile_blueprint"):
            unreal.BlueprintEditorLibrary.compile_blueprint(bp)
            if hasattr(bp, "status"):
                st = bp.status
                if st == unreal.BlueprintStatus.BS_UP_TO_DATE:
                    compile_status = "UpToDate"
                elif st == unreal.BlueprintStatus.BS_COMPILED:
                    compile_status = "Compiled"
                elif st == unreal.BlueprintStatus.BS_ERROR:
                    compile_status = "Error"
                else:
                    compile_status = str(st)
            else:
                compile_status = "Compiled"
    except Exception as e:
        compile_status = "Skipped"
        messages.append(f"compile_skipped: {e}")

    if VERBOSE_LOG:
        unreal.log(f"[Exporter] {asset_name} [{kind_label}]: graphs={len(graphs)} nodes={total_nodes}")

    doc = {
        "asset_name": asset_name,
        "asset_path": asset_path,
        "generated_class": generated_class_path,
        "last_exported": now_iso(),
        "variables": variables,
        "functions": functions,
        "macros": macros,
        "graphs": graphs,
        "compile_status": compile_status,
        "messages": messages
    }
    doc["hash"] = make_hash({
        "variables": variables,
        "functions": functions,
        "macros": macros,
        "graphs": graphs
    })
    return doc

def run_export():
    OUTPUT_DIR.mkdir(parents=True, exist_ok=True)

    registry = unreal.AssetRegistryHelpers.get_asset_registry()
    assets = registry.get_assets_by_path("/Game", recursive=True)

    # Tomamos cualquier cosa cuyo AssetClass contenga "Blueprint"
    candidates = []
    for a in assets:
        try:
            cls = a.asset_class_path.asset_name  # p.ej. 'Blueprint', 'WidgetBlueprint', 'AnimBlueprint', ...
        except Exception:
            cls = ""
        if "Blueprint" in str(cls):
            candidates.append(a)

    export_all = []
    with OUTPUT_NDJSON.open("w", encoding="utf-8") as nd:
        for a in candidates:
            obj, load_path = _load_asset_by_ad(a)
            if obj is None:
                unreal.log_warning(f"[Exporter] No se pudo cargar asset: {a.package_name}.{a.asset_name}")
                continue

            bp_like, kind = _normalize_to_blueprint(obj)
            if bp_like is None:
                # si acá llegamos, el asset es un Blueprint pero no tenemos el objeto de editor
                unreal.log_warning(f"[Exporter] {a.asset_name}: tipo no soportado para exportar grafos ({kind}). Ruta: {load_path}")
                continue

            try:
                doc = export_blueprint(bp_like, kind_label=kind)
                export_all.append(doc)
                nd.write(json.dumps(doc, ensure_ascii=False) + "\n")
            except Exception as e:
                unreal.log_warning(f"[Exporter] Error exportando {a.asset_name}: {e}")

    with OUTPUT_JSON.open("w", encoding="utf-8") as f:
        json.dump({
            "project": get_project_name_safe(),
            "exported_at": now_iso(),
            "count": len(export_all),
            "blueprints": export_all
        }, f, ensure_ascii=False, indent=2)

    unreal.log(f"[Exporter] Export listo: {str(OUTPUT_JSON)}")
    unreal.log(f"[Exporter] NDJSON: {str(OUTPUT_NDJSON)}")

if __name__ == "__main__":
    run_export()
