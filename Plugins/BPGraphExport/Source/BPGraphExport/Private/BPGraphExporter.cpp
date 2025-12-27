#include "BPGraphExporter.h"

#include "Misc/Paths.h"
#include "Misc/FileHelper.h"
#include "HAL/FileManager.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "EdGraph/EdGraph.h"
#include "EdGraph/EdGraphNode.h"
#include "EdGraph/EdGraphPin.h"
#include "Engine/Blueprint.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonWriter.h"
#include "Serialization/JsonSerializer.h"
#include "Misc/SecureHash.h"
#include "Misc/App.h"

// ------------------------------------------------------------
// Helpers
// ------------------------------------------------------------
static FString NowISO()
{
    return FDateTime::Now().ToIso8601();
}

FString FBPGraphExporter::GetOutputDir()
{
    return FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir() / TEXT("BlueprintExports"));
}

FString FBPGraphExporter::Serialize(const TSharedRef<FJsonObject>& Obj)
{
    FString Out;
    const TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&Out);
    FJsonSerializer::Serialize(Obj, Writer);
    return Out;
}

FString FBPGraphExporter::HashString(const FString& In)
{
    return FMD5::HashAnsiString(*In);
}

static FString PinTypeToStr(const FEdGraphPinType& T)
{
    const FString Cat = T.PinCategory.ToString();
    const FString Sub = T.PinSubCategory.ToString();
    UObject* SubObj = T.PinSubCategoryObject.Get();
    const FString Obj = SubObj ? SubObj->GetName() : TEXT("");

    FString Out = Cat;
    if (!Sub.IsEmpty()) Out += TEXT("::") + Sub;
    if (!Obj.IsEmpty()) Out += TEXT("::") + Obj;
    return Out;
}

// ------------------------------------------------------------
// Export a single graph
// ------------------------------------------------------------
TSharedRef<FJsonObject> FBPGraphExporter::ExportGraph(UEdGraph* Graph)
{
    TSharedRef<FJsonObject> J = MakeShared<FJsonObject>();
    if (!Graph)
    {
        J->SetArrayField(TEXT("nodes"), {});
        J->SetArrayField(TEXT("connections"), {});
        return J;
    }

    J->SetStringField(TEXT("name"), Graph->GetName());
    J->SetStringField(TEXT("schema"), (Graph->Schema ? Graph->Schema->GetName() : TEXT("")));

    TArray<TSharedPtr<FJsonValue>> Nodes;
    TArray<TSharedPtr<FJsonValue>> Connections;
    TSet<FString> SeenEdges;

    for (UEdGraphNode* N : Graph->Nodes)
    {
        if (!N) continue;

        TSharedRef<FJsonObject> JN = MakeShared<FJsonObject>();
        JN->SetStringField(TEXT("id"), N->GetName());
        JN->SetStringField(TEXT("title"), N->GetNodeTitle(ENodeTitleType::FullTitle).ToString());
        JN->SetStringField(TEXT("class"), N->GetClass()->GetName());

        // Posición
        {
            TArray<TSharedPtr<FJsonValue>> Pos;
            Pos.Add(MakeShared<FJsonValueNumber>(N->NodePosX));
            Pos.Add(MakeShared<FJsonValueNumber>(N->NodePosY));
            JN->SetArrayField(TEXT("pos"), Pos);
        }

        // Pines
        TArray<TSharedPtr<FJsonValue>> Pins;
        for (UEdGraphPin* P : N->Pins)
        {
            if (!P) continue;
            const bool bInput = (P->Direction == EGPD_Input);

            TSharedRef<FJsonObject> JP = MakeShared<FJsonObject>();
            JP->SetStringField(TEXT("name"), P->PinName.ToString());
            JP->SetStringField(TEXT("direction"), bInput ? TEXT("input") : TEXT("output"));
            JP->SetStringField(TEXT("type"), PinTypeToStr(P->PinType));
            JP->SetStringField(TEXT("default_value"), P->DefaultValue);

            // Links
            TArray<TSharedPtr<FJsonValue>> Linked;
            for (UEdGraphPin* L : P->LinkedTo)
            {
                if (!L) continue;

                TSharedRef<FJsonObject> JL = MakeShared<FJsonObject>();
                JL->SetStringField(TEXT("node"), L->GetOwningNode()->GetName());
                JL->SetStringField(TEXT("pin"), L->PinName.ToString());
                Linked.Add(MakeShared<FJsonValueObject>(JL));

                // Edge deduplicado
                FString FromNode = N->GetName();
                FString FromPin = P->PinName.ToString();
                FString ToNode = L->GetOwningNode()->GetName();
                FString ToPin = L->PinName.ToString();

                const bool LIsInput = (L->Direction == EGPD_Input);
                if (bInput && !LIsInput)
                {
                    Swap(FromNode, ToNode);
                    Swap(FromPin, ToPin);
                }

                const FString Key = FString::Printf(TEXT("%s|%s->%s|%s"), *FromNode, *FromPin, *ToNode, *ToPin);
                if (!SeenEdges.Contains(Key))
                {
                    SeenEdges.Add(Key);
                    TSharedRef<FJsonObject> Edge = MakeShared<FJsonObject>();
                    TSharedRef<FJsonObject> O1 = MakeShared<FJsonObject>();
                    O1->SetStringField(TEXT("node"), FromNode);
                    O1->SetStringField(TEXT("pin"), FromPin);
                    Edge->SetObjectField(TEXT("from"), O1);

                    TSharedRef<FJsonObject> O2 = MakeShared<FJsonObject>();
                    O2->SetStringField(TEXT("node"), ToNode);
                    O2->SetStringField(TEXT("pin"), ToPin);
                    Edge->SetObjectField(TEXT("to"), O2);

                    Edge->SetBoolField(TEXT("is_exec"), P->PinType.PinCategory.ToString().Equals(TEXT("exec"), ESearchCase::IgnoreCase));
                    Connections.Add(MakeShared<FJsonValueObject>(Edge));
                }
            }
            JP->SetArrayField(TEXT("linked_to"), Linked);
            Pins.Add(MakeShared<FJsonValueObject>(JP));
        }

        JN->SetArrayField(TEXT("pins"), Pins);
        Nodes.Add(MakeShared<FJsonValueObject>(JN));
    }

    J->SetArrayField(TEXT("nodes"), Nodes);
    J->SetArrayField(TEXT("connections"), Connections);
    return J;
}

// ------------------------------------------------------------
// Export a single Blueprint
// ------------------------------------------------------------
bool FBPGraphExporter::ExportOne(UBlueprint* BP, TSharedRef<FJsonObject>& OutJson)
{
    if (!BP) return false;

    TArray<TSharedPtr<FJsonValue>> Graphs;
    auto AddGraphs = [&](const TArray<UEdGraph*>& Arr)
    {
        for (UEdGraph* G : Arr)
            if (G)
                Graphs.Add(MakeShared<FJsonValueObject>(ExportGraph(G)));
    };

    AddGraphs(BP->UbergraphPages);
    AddGraphs(BP->FunctionGraphs);
    AddGraphs(BP->MacroGraphs);

    // Variables
    TArray<TSharedPtr<FJsonValue>> Vars;
    for (const FBPVariableDescription& V : BP->NewVariables)
    {
        TSharedRef<FJsonObject> JV = MakeShared<FJsonObject>();
        JV->SetStringField(TEXT("name"), V.VarName.ToString());
        JV->SetStringField(TEXT("type"), PinTypeToStr(V.VarType));
        Vars.Add(MakeShared<FJsonValueObject>(JV));
    }

    OutJson->SetStringField(TEXT("asset_name"), BP->GetName());
    OutJson->SetStringField(TEXT("asset_path"), BP->GetPathName());
    OutJson->SetStringField(TEXT("generated_class"), BP->GeneratedClass ? BP->GeneratedClass->GetPathName() : TEXT(""));
    OutJson->SetStringField(TEXT("last_exported"), NowISO());
    OutJson->SetArrayField(TEXT("variables"), Vars);
    OutJson->SetArrayField(TEXT("graphs"), Graphs);
    OutJson->SetStringField(TEXT("compile_status"), TEXT("Compiled"));
    OutJson->SetArrayField(TEXT("messages"), {});
    OutJson->SetStringField(TEXT("hash"), HashString(Serialize(OutJson)));

    return true;
}

// ------------------------------------------------------------
// Export all Blueprints
// ------------------------------------------------------------
bool FBPGraphExporter::ExportAll()
{
    const FString Dir = GetOutputDir();
    IFileManager::Get().MakeDirectory(*Dir, true);

    // Buscar Blueprints
    TArray<FAssetData> Assets;
    FAssetRegistryModule& ARM = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");
    ARM.Get().GetAssetsByPath(FName("/Game"), Assets, true);

    TArray<TSharedPtr<FJsonValue>> Docs;

    // Exportar cada uno
    for (const FAssetData& A : Assets)
    {
        UBlueprint* BP = Cast<UBlueprint>(A.GetAsset());
        if (!BP) continue;

        TSharedRef<FJsonObject> Doc = MakeShared<FJsonObject>();
        if (ExportOne(BP, Doc))
            Docs.Add(MakeShared<FJsonValueObject>(Doc));
    }

    // JSON grande (todo junto)
    TSharedRef<FJsonObject> Root = MakeShared<FJsonObject>();
    Root->SetStringField(TEXT("project"), FApp::GetProjectName());
    Root->SetStringField(TEXT("exported_at"), NowISO());
    Root->SetNumberField(TEXT("count"), Docs.Num());
    Root->SetArrayField(TEXT("blueprints"), Docs);

    const FString OutStr = Serialize(Root);
    const FString JsonPath = Dir / TEXT("blueprints.json");
    FFileHelper::SaveStringToFile(OutStr, *JsonPath, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);

    // NDJSON — con graphs completos
    const FString NdjsonPath = Dir / TEXT("blueprints.ndjson");
    IFileManager::Get().Delete(*NdjsonPath);

    for (const TSharedPtr<FJsonValue>& V : Docs)
    {
        const TSharedPtr<FJsonObject> Obj = V->AsObject();
        if (!Obj.IsValid()) continue;

        FString Line;
        const TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&Line);
        FJsonSerializer::Serialize(Obj.ToSharedRef(), Writer);
        Line += TEXT("\n");

        FFileHelper::SaveStringToFile(
            Line, *NdjsonPath,
            FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM,
            &IFileManager::Get(),
            FILEWRITE_Append
        );
    }

    UE_LOG(LogTemp, Display, TEXT("[BPGraphExport] Export listo: %s"), *JsonPath);
    UE_LOG(LogTemp, Display, TEXT("[BPGraphExport] NDJSON listo: %s"), *NdjsonPath);
    return true;
}
