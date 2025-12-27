#pragma once
#include "CoreMinimal.h"

class FBPGraphExporter
{
public:
    static FString GetOutputDir();
    static bool ExportAll();
private:
    static bool ExportOne(class UBlueprint* BP, TSharedRef<class FJsonObject>& OutJson);
    static TSharedRef<class FJsonObject> ExportGraph(class UEdGraph* Graph);
    static FString Serialize(const TSharedRef<class FJsonObject>& Obj);
    static FString HashString(const FString& In);
};
