#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "PacMan.h"

#include "PacManGameModeBase.generated.h"

class ACameraActor;

UCLASS()
class PACMAN_API APacmanGameModeBase : public AGameModeBase
{
    GENERATED_BODY()

public:
    APacmanGameModeBase();

    virtual void BeginPlay() override;
    virtual void Tick(float DeltaSeconds) override;

    void CollectPellet();
    void LoseGame();
    void RestartGame();

    int32 GetScore() const;
    int32 GetPelletsRemaining() const;
    float GetElapsedTime() const;

    EPacmanGameState GetGameState() const;
    bool IsRoundActive() const;

    APacmanPlayer* GetPacmanPlayer() const;

    FVector GetGhostTarget(
        EPacmanGhostPattern Pattern,
        const FVector& GhostLocation) const;

    FVector GetGhostWaypoint(
        EPacmanGhostPattern Pattern,
        const FVector& GhostLocation) const;

    FIntPoint FindNextCell(
        const FIntPoint& Start,
        const FIntPoint& Goal) const;

private:
    UPROPERTY()
    APacmanPlayer* Player;

    UPROPERTY()
    UPacmanHUDWidget* HUD;

    UPROPERTY()
    ACameraActor* GameCamera;

    UPROPERTY()
    TArray<APacmanGhost*> Ghosts;

    UPROPERTY()
    TArray<AActor*> GeneratedActors;

    TArray<FString> MapLayout;

    int32 MapRows;
    int32 MapColumns;
    float CellSize;

    int32 Score;
    int32 TotalPellets;
    int32 CollectedPellets;

    float StartTime;
    float EndTime;

    bool bRestarting;
    EPacmanGameState CurrentState;

    void BuildRound();
    void GenerateMaze();
    void BuildMap();
    void SpawnPellets();
    void SpawnGhosts();
    void SpawnHUD();
    void ClearGeneratedActors();

    void SetupGameCamera();
    void SetGameState(EPacmanGameState NewState);

    void SpawnBlock(
        const FVector& Location,
        const FVector& Scale,
        const FLinearColor& Color);

    bool IsWalkable(int32 Row, int32 Column) const;

    FVector CellToWorld(int32 Row, int32 Column) const;
    FIntPoint WorldToCell(const FVector& Location) const;
    FIntPoint FindNearestWalkable(const FIntPoint& DesiredCell) const;
};