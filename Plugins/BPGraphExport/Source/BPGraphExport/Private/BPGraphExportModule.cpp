#include "Modules/ModuleManager.h"
#include "ToolMenus.h"
#include "BPGraphExporter.h"

class FBPGraphExportModule : public IModuleInterface
{
public:
    virtual void StartupModule() override
    {
        UToolMenus::RegisterStartupCallback(
            FSimpleMulticastDelegate::FDelegate::CreateRaw(this, &FBPGraphExportModule::RegisterMenus)
        );
    }

    virtual void ShutdownModule() override {}

private:
    void RegisterMenus()
    {
        UToolMenu* Menu = UToolMenus::Get()->ExtendMenu("LevelEditor.MainMenu.File");
        FToolMenuSection& Section = Menu->AddSection("BPGraphExportSection", FText::FromString("Blueprint Graph Export"));
        Section.AddMenuEntry(
            "ExportAllBPGraphs",
            FText::FromString("Export All Blueprints (Graphs) to JSON"),
            FText::FromString("Exporta nodos y conexiones de todos los Blueprints."),
            FSlateIcon(),
            FUIAction(FExecuteAction::CreateRaw(this, &FBPGraphExportModule::DoExportAll))
        );
    }

    void DoExportAll()
    {
        if (FBPGraphExporter::ExportAll())
        {
            UE_LOG(LogTemp, Display, TEXT("[BPGraphExport] Export completado en %s"), *FBPGraphExporter::GetOutputDir());
        }
        else
        {
            UE_LOG(LogTemp, Warning, TEXT("[BPGraphExport] Falló exportación."));
        }
    }
};

IMPLEMENT_MODULE(FBPGraphExportModule, BPGraphExport)
