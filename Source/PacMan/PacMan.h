#pragma once

#include "CoreMinimal.h"
#include "AIController.h"
#include "Blueprint/UserWidget.h"
#include "GameFramework/Actor.h"
#include "GameFramework/Character.h"
#include "GameFramework/Pawn.h"

#include "PacMan.generated.h"

class UButton;
class UFloatingPawnMovement;
class USphereComponent;
class UStaticMeshComponent;
class UTextBlock;

UENUM(BlueprintType)
enum class EPacmanGameState : uint8
{
    Playing,
    Won,
    Lost
};

UENUM(BlueprintType)
enum class EPacmanGhostPattern : uint8
{
    Chase,
    Ambush,
    Flank
};

UCLASS()
class PACMAN_API APacmanPlayer : public APawn
{
    GENERATED_BODY()

public:
    APacmanPlayer();

    virtual void BeginPlay() override;
    virtual void Tick(float DeltaSeconds) override;

    void ResetPlayer(const FVector& NewLocation);
    void StopPlayer();

private:
    UPROPERTY(VisibleAnywhere)
    USphereComponent* Collision;

    UPROPERTY(VisibleAnywhere)
    UStaticMeshComponent* SphereMesh;

    UPROPERTY(VisibleAnywhere)
    UFloatingPawnMovement* Movement;

    float MoveSpeed;
};

UCLASS()
class PACMAN_API APacmanPellet : public AActor
{
    GENERATED_BODY()

public:
    APacmanPellet();

    virtual void BeginPlay() override;

private:
    UPROPERTY(VisibleAnywhere)
    USphereComponent* Collision;

    UPROPERTY(VisibleAnywhere)
    UStaticMeshComponent* SphereMesh;

    bool bCollected;

    UFUNCTION()
    void OnPelletOverlap(
        UPrimitiveComponent* OverlappedComponent,
        AActor* OtherActor,
        UPrimitiveComponent* OtherComp,
        int32 OtherBodyIndex,
        bool bFromSweep,
        const FHitResult& SweepResult);
};

UCLASS()
class PACMAN_API APacmanGhost : public ACharacter
{
    GENERATED_BODY()

public:
    APacmanGhost();

    virtual void BeginPlay() override;

    void ConfigureGhost(
        int32 NewIndex,
        EPacmanGhostPattern NewPattern,
        const FLinearColor& NewColor);

    void StopGhost();

    int32 GetGhostIndex() const;
    EPacmanGhostPattern GetGhostPattern() const;

private:
    UPROPERTY(VisibleAnywhere)
    UStaticMeshComponent* SphereMesh;

    int32 GhostIndex;
    EPacmanGhostPattern Pattern;
    FLinearColor GhostColor;

    UFUNCTION()
    void OnGhostOverlap(
        UPrimitiveComponent* OverlappedComponent,
        AActor* OtherActor,
        UPrimitiveComponent* OtherComp,
        int32 OtherBodyIndex,
        bool bFromSweep,
        const FHitResult& SweepResult);
};

UCLASS()
class PACMAN_API APacmanAIController : public AAIController
{
    GENERATED_BODY()

public:
    APacmanAIController();

    virtual void OnPossess(APawn* InPawn) override;
    virtual void Tick(float DeltaSeconds) override;

private:
    FVector CurrentWaypoint;

    float RepathTimer;
    float StuckTimer;

    bool bHasWaypoint;
    bool bManualMovement;

    FVector PreviousLocation;
};

UCLASS()
class PACMAN_API UPacmanHUDWidget : public UUserWidget
{
    GENERATED_BODY()

public:
    virtual TSharedRef<SWidget> RebuildWidget() override;
    virtual void NativeConstruct() override;
    virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

private:
    UPROPERTY()
    UTextBlock* ScoreText;

    UPROPERTY()
    UTextBlock* TimeText;

    UPROPERTY()
    UTextBlock* PelletsText;

    UPROPERTY()
    UTextBlock* StateText;

    UPROPERTY()
    UButton* RestartButton;

    UFUNCTION()
    void RestartClicked();

    void UpdateInterface();
};