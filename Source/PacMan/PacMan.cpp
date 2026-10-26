#include "PacMan.h"
#include "PacManGameModeBase.h"

#include "Blueprint/WidgetTree.h"

#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"

#include "Engine/StaticMesh.h"
#include "Engine/World.h"

#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/FloatingPawnMovement.h"
#include "GameFramework/PlayerController.h"

#include "InputCoreTypes.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "Modules/ModuleManager.h"
#include "Navigation/PathFollowingComponent.h"
#include "NavigationSystem.h"
#include "Styling/CoreStyle.h"
#include "Styling/SlateColor.h"
#include "UObject/ConstructorHelpers.h"

namespace
{
    void ApplyColor(UStaticMeshComponent* MeshComponent, const FLinearColor& Color)
    {
        if (!MeshComponent)
        {
            return;
        }

        UMaterialInstanceDynamic* Material =
            MeshComponent->CreateAndSetMaterialInstanceDynamic(0);

        if (Material)
        {
            Material->SetVectorParameterValue(TEXT("Color"), Color);
        }
    }
}

APacmanPlayer::APacmanPlayer()
{
    PrimaryActorTick.bCanEverTick = true;
    MoveSpeed = 420.0f;

    Collision = CreateDefaultSubobject<USphereComponent>(TEXT("Collision"));
    RootComponent = Collision;

    Collision->InitSphereRadius(40.0f);
    Collision->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
    Collision->SetCollisionObjectType(ECC_Pawn);
    Collision->SetCollisionResponseToAllChannels(ECR_Block);
    Collision->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
    Collision->SetCollisionResponseToChannel(ECC_Visibility, ECR_Ignore);
    Collision->SetGenerateOverlapEvents(true);
    Collision->SetCanEverAffectNavigation(false);

    SphereMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("SphereMesh"));
    SphereMesh->SetupAttachment(RootComponent);
    SphereMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    SphereMesh->SetCanEverAffectNavigation(false);
    SphereMesh->SetRelativeScale3D(FVector(0.8f));

    static ConstructorHelpers::FObjectFinder<UStaticMesh> SphereAsset(
        TEXT("/Engine/BasicShapes/Sphere.Sphere"));

    if (SphereAsset.Succeeded())
    {
        SphereMesh->SetStaticMesh(SphereAsset.Object);
    }

    static ConstructorHelpers::FObjectFinder<UMaterialInterface> ColorMaterial(
        TEXT("/Game/Materials/M_PacmanColor.M_PacmanColor"));

    if (ColorMaterial.Succeeded())
    {
        SphereMesh->SetMaterial(0, ColorMaterial.Object);
    }

    Movement = CreateDefaultSubobject<UFloatingPawnMovement>(TEXT("Movement"));
    Movement->SetUpdatedComponent(Collision);
    Movement->MaxSpeed = MoveSpeed;
    Movement->Acceleration = 1000000.0f;
    Movement->Deceleration = 1000000.0f;
    Movement->TurningBoost = 0.0f;
    Movement->SetPlaneConstraintEnabled(true);
    Movement->SetPlaneConstraintNormal(FVector::UpVector);

    AutoPossessPlayer = EAutoReceiveInput::Disabled;
}

void APacmanPlayer::BeginPlay()
{
    Super::BeginPlay();
    ApplyColor(SphereMesh, FLinearColor(1.0f, 0.85f, 0.0f, 1.0f));
}

void APacmanPlayer::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);

    APacmanGameModeBase* GameMode =
        GetWorld()->GetAuthGameMode<APacmanGameModeBase>();

    APlayerController* PC = Cast<APlayerController>(GetController());

    if (!GameMode || !PC || !GameMode->IsRoundActive())
    {
        StopPlayer();
        return;
    }

    FVector Direction = FVector::ZeroVector;

    if (PC->IsInputKeyDown(EKeys::W) || PC->IsInputKeyDown(EKeys::Up))
    {
        Direction.X += 1.0f;
    }

    if (PC->IsInputKeyDown(EKeys::S) || PC->IsInputKeyDown(EKeys::Down))
    {
        Direction.X -= 1.0f;
    }

    if (PC->IsInputKeyDown(EKeys::D) || PC->IsInputKeyDown(EKeys::Right))
    {
        Direction.Y += 1.0f;
    }

    if (PC->IsInputKeyDown(EKeys::A) || PC->IsInputKeyDown(EKeys::Left))
    {
        Direction.Y -= 1.0f;
    }

    Direction = Direction.GetSafeNormal();

    if (Direction.IsNearlyZero())
    {
        StopPlayer();
        return;
    }

    Movement->Velocity = Direction * MoveSpeed;
    AddMovementInput(Direction, 1.0f);
}

void APacmanPlayer::StopPlayer()
{
    ConsumeMovementInputVector();

    if (Movement)
    {
        Movement->StopMovementImmediately();
    }
}

void APacmanPlayer::ResetPlayer(const FVector& NewLocation)
{
    StopPlayer();

    SetActorLocation(NewLocation, false, nullptr, ETeleportType::TeleportPhysics);
    SetActorRotation(FRotator::ZeroRotator);

    if (Movement)
    {
        Movement->SetPlaneConstraintOrigin(NewLocation);
    }

    SetActorTickEnabled(true);
}

APacmanPellet::APacmanPellet()
{
    PrimaryActorTick.bCanEverTick = false;
    bCollected = false;

    Collision = CreateDefaultSubobject<USphereComponent>(TEXT("Collision"));
    RootComponent = Collision;

    Collision->InitSphereRadius(28.0f);
    Collision->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
    Collision->SetCollisionObjectType(ECC_WorldDynamic);
    Collision->SetCollisionResponseToAllChannels(ECR_Ignore);
    Collision->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
    Collision->SetGenerateOverlapEvents(true);
    Collision->SetCanEverAffectNavigation(false);

    Collision->OnComponentBeginOverlap.AddDynamic(
        this, &APacmanPellet::OnPelletOverlap);

    SphereMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("SphereMesh"));
    SphereMesh->SetupAttachment(RootComponent);
    SphereMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    SphereMesh->SetCanEverAffectNavigation(false);
    SphereMesh->SetRelativeScale3D(FVector(0.22f));

    static ConstructorHelpers::FObjectFinder<UStaticMesh> SphereAsset(
        TEXT("/Engine/BasicShapes/Sphere.Sphere"));

    if (SphereAsset.Succeeded())
    {
        SphereMesh->SetStaticMesh(SphereAsset.Object);
    }

    static ConstructorHelpers::FObjectFinder<UMaterialInterface> ColorMaterial(
        TEXT("/Game/Materials/M_PacmanColor.M_PacmanColor"));

    if (ColorMaterial.Succeeded())
    {
        SphereMesh->SetMaterial(0, ColorMaterial.Object);
    }
}

void APacmanPellet::BeginPlay()
{
    Super::BeginPlay();
    ApplyColor(SphereMesh, FLinearColor(1.0f, 0.75f, 0.08f, 1.0f));
}

void APacmanPellet::OnPelletOverlap(
    UPrimitiveComponent* OverlappedComponent,
    AActor* OtherActor,
    UPrimitiveComponent* OtherComp,
    int32 OtherBodyIndex,
    bool bFromSweep,
    const FHitResult& SweepResult)
{
    if (bCollected || !Cast<APacmanPlayer>(OtherActor))
    {
        return;
    }

    APacmanGameModeBase* GameMode =
        GetWorld()->GetAuthGameMode<APacmanGameModeBase>();

    if (!GameMode || !GameMode->IsRoundActive())
    {
        return;
    }

    bCollected = true;
    SetActorEnableCollision(false);
    SetActorHiddenInGame(true);

    GameMode->CollectPellet();
    Destroy();
}

APacmanGhost::APacmanGhost()
{
    PrimaryActorTick.bCanEverTick = false;

    GhostIndex = 0;
    Pattern = EPacmanGhostPattern::Chase;
    GhostColor = FLinearColor::Red;

    UCapsuleComponent* Capsule = GetCapsuleComponent();

    Capsule->InitCapsuleSize(36.0f, 48.0f);
    Capsule->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
    Capsule->SetCollisionObjectType(ECC_Pawn);
    Capsule->SetCollisionResponseToAllChannels(ECR_Block);
    Capsule->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
    Capsule->SetCollisionResponseToChannel(ECC_Visibility, ECR_Ignore);
    Capsule->SetGenerateOverlapEvents(true);
    Capsule->SetCanEverAffectNavigation(false);

    Capsule->OnComponentBeginOverlap.AddDynamic(
        this, &APacmanGhost::OnGhostOverlap);

    GetMesh()->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    GetMesh()->SetCanEverAffectNavigation(false);

    SphereMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("SphereMesh"));
    SphereMesh->SetupAttachment(Capsule);
    SphereMesh->SetRelativeScale3D(FVector(0.76f));
    SphereMesh->SetRelativeLocation(FVector(0.0f, 0.0f, -8.0f));
    SphereMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    SphereMesh->SetCanEverAffectNavigation(false);

    static ConstructorHelpers::FObjectFinder<UStaticMesh> SphereAsset(
        TEXT("/Engine/BasicShapes/Sphere.Sphere"));

    if (SphereAsset.Succeeded())
    {
        SphereMesh->SetStaticMesh(SphereAsset.Object);
    }

    static ConstructorHelpers::FObjectFinder<UMaterialInterface> ColorMaterial(
        TEXT("/Game/Materials/M_PacmanColor.M_PacmanColor"));

    if (ColorMaterial.Succeeded())
    {
        SphereMesh->SetMaterial(0, ColorMaterial.Object);
    }

    UCharacterMovementComponent* GhostMovement = GetCharacterMovement();

    GhostMovement->MaxWalkSpeed = 270.0f;
    GhostMovement->MaxAcceleration = 10000.0f;
    GhostMovement->BrakingDecelerationWalking = 10000.0f;
    GhostMovement->GroundFriction = 12.0f;
    GhostMovement->bOrientRotationToMovement = false;
    GhostMovement->bUseRVOAvoidance = false;
    GhostMovement->NavAgentProps.AgentRadius = 36.0f;
    GhostMovement->NavAgentProps.AgentHeight = 96.0f;

    bUseControllerRotationYaw = false;
    AIControllerClass = APacmanAIController::StaticClass();
    AutoPossessAI = EAutoPossessAI::PlacedInWorldOrSpawned;
}

void APacmanGhost::BeginPlay()
{
    Super::BeginPlay();
    ApplyColor(SphereMesh, GhostColor);
}

void APacmanGhost::ConfigureGhost(
    int32 NewIndex,
    EPacmanGhostPattern NewPattern,
    const FLinearColor& NewColor)
{
    GhostIndex = NewIndex;
    Pattern = NewPattern;
    GhostColor = NewColor;

    ApplyColor(SphereMesh, GhostColor);

    const float Speeds[] = {270.0f, 250.0f, 260.0f};
    GetCharacterMovement()->MaxWalkSpeed = Speeds[FMath::Clamp(GhostIndex, 0, 2)];
}

int32 APacmanGhost::GetGhostIndex() const
{
    return GhostIndex;
}

EPacmanGhostPattern APacmanGhost::GetGhostPattern() const
{
    return Pattern;
}

void APacmanGhost::StopGhost()
{
    AAIController* AI = Cast<AAIController>(GetController());

    if (AI)
    {
        AI->StopMovement();
    }

    GetCharacterMovement()->StopMovementImmediately();
}

void APacmanGhost::OnGhostOverlap(
    UPrimitiveComponent* OverlappedComponent,
    AActor* OtherActor,
    UPrimitiveComponent* OtherComp,
    int32 OtherBodyIndex,
    bool bFromSweep,
    const FHitResult& SweepResult)
{
    if (!Cast<APacmanPlayer>(OtherActor))
    {
        return;
    }

    APacmanGameModeBase* GameMode =
        GetWorld()->GetAuthGameMode<APacmanGameModeBase>();

    if (GameMode && GameMode->IsRoundActive())
    {
        GameMode->LoseGame();
    }
}

APacmanAIController::APacmanAIController()
{
    PrimaryActorTick.bCanEverTick = true;

    CurrentWaypoint = FVector::ZeroVector;
    PreviousLocation = FVector::ZeroVector;

    RepathTimer = 0.0f;
    StuckTimer = 0.0f;

    bHasWaypoint = false;
    bManualMovement = false;
}

void APacmanAIController::OnPossess(APawn* InPawn)
{
    Super::OnPossess(InPawn);

    RepathTimer = 0.0f;
    StuckTimer = 0.0f;

    bHasWaypoint = false;
    bManualMovement = false;

    CurrentWaypoint = FVector::ZeroVector;
    PreviousLocation = InPawn ? InPawn->GetActorLocation() : FVector::ZeroVector;
}

void APacmanAIController::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);

    APacmanGameModeBase* GameMode =
        GetWorld()->GetAuthGameMode<APacmanGameModeBase>();

    APacmanGhost* Ghost = Cast<APacmanGhost>(GetPawn());

    if (!GameMode || !Ghost)
    {
        return;
    }

    if (!GameMode->IsRoundActive())
    {
        StopMovement();

        Ghost->ConsumeMovementInputVector();
        Ghost->GetCharacterMovement()->StopMovementImmediately();

        bHasWaypoint = false;
        bManualMovement = false;
        StuckTimer = 0.0f;
        PreviousLocation = Ghost->GetActorLocation();
        return;
    }

    APacmanPlayer* Player = GameMode->GetPacmanPlayer();

    if (!IsValid(Player))
    {
        return;
    }

    const FVector GhostLocation = Ghost->GetActorLocation();
    const float MovementDistance = FVector::Dist2D(GhostLocation, PreviousLocation);

    PreviousLocation = GhostLocation;

    const float DistanceToWaypoint = bHasWaypoint
        ? FVector::Dist2D(GhostLocation, CurrentWaypoint)
        : 0.0f;

    const bool bReachedWaypoint = bHasWaypoint && DistanceToWaypoint <= 12.0f;

    if (bHasWaypoint && !bReachedWaypoint && MovementDistance < 0.5f)
    {
        StuckTimer += DeltaSeconds;
    }
    else
    {
        StuckTimer = 0.0f;
    }

    if (!bManualMovement && StuckTimer > 0.8f)
    {
        StopMovement();

        bManualMovement = true;
        RepathTimer = 0.0f;
        StuckTimer = 0.0f;
    }

    if (bManualMovement && StuckTimer > 0.8f)
    {
        bHasWaypoint = false;
        RepathTimer = 0.0f;
        StuckTimer = 0.0f;
    }

    RepathTimer -= DeltaSeconds;

    if (!bHasWaypoint || bReachedWaypoint || RepathTimer <= 0.0f)
    {
        const bool bWasManual = bManualMovement;
        const bool bKeepManualWaypoint =
            bWasManual && bHasWaypoint && !bReachedWaypoint;

        if (!bKeepManualWaypoint)
        {
            CurrentWaypoint = GameMode->GetGhostWaypoint(
                Ghost->GetGhostPattern(), GhostLocation);
        }

        bHasWaypoint = true;

        FCollisionQueryParams QueryParams;
        QueryParams.AddIgnoredActor(Ghost);
        QueryParams.AddIgnoredActor(Player);

        FHitResult Hit;

        const bool bWallBetween = GetWorld()->LineTraceSingleByChannel(
            Hit,
            GhostLocation,
            Player->GetActorLocation(),
            ECC_Visibility,
            QueryParams);

        FVector NavigationTarget = CurrentWaypoint;

        if (!bWallBetween &&
            Ghost->GetGhostPattern() == EPacmanGhostPattern::Chase)
        {
            NavigationTarget = Player->GetActorLocation();
        }

        UNavigationSystemV1* NavSystem =
            FNavigationSystem::GetCurrent<UNavigationSystemV1>(GetWorld());

        bool bCanUseNavigation = false;
        FNavLocation ProjectedTarget;
        FNavLocation ProjectedStart;

        if (NavSystem)
        {
            const FVector ProjectionExtent(60.0f, 60.0f, 200.0f);

            const bool bStartOnNavMesh = NavSystem->ProjectPointToNavigation(
                GhostLocation, ProjectedStart, ProjectionExtent);

            const bool bTargetOnNavMesh = NavSystem->ProjectPointToNavigation(
                NavigationTarget, ProjectedTarget, ProjectionExtent);

            bCanUseNavigation = bStartOnNavMesh && bTargetOnNavMesh;
        }

        bool bNavigationAccepted = false;
        const bool bMaySwitchToNavigation = !bWasManual || !bKeepManualWaypoint;

        if (bCanUseNavigation && bMaySwitchToNavigation)
        {
            const EPathFollowingRequestResult::Type Result = MoveToLocation(
                ProjectedTarget.Location,
                8.0f,
                false,
                true,
                true,
                nullptr,
                false);

            bNavigationAccepted = Result != EPathFollowingRequestResult::Failed;

            if (Result == EPathFollowingRequestResult::AlreadyAtGoal &&
                FVector::Dist2D(GhostLocation, CurrentWaypoint) > 16.0f)
            {
                bNavigationAccepted = false;
            }
        }

        if (bNavigationAccepted)
        {
            bManualMovement = false;
            RepathTimer = 0.45f;
        }
        else
        {
            StopMovement();

            bManualMovement = true;
            RepathTimer = 0.45f;
        }
    }

    if (!bManualMovement || !bHasWaypoint)
    {
        return;
    }

    FVector Direction = CurrentWaypoint - Ghost->GetActorLocation();
    Direction.Z = 0.0f;

    const float Distance = Direction.Size();

    if (Distance <= 8.0f)
    {
        Ghost->ConsumeMovementInputVector();
        Ghost->GetCharacterMovement()->StopMovementImmediately();

        bHasWaypoint = false;
        return;
    }

    Direction /= Distance;
    Ghost->AddMovementInput(Direction, 1.0f);
}

TSharedRef<SWidget> UPacmanHUDWidget::RebuildWidget()
{
    if (!WidgetTree)
    {
        WidgetTree = NewObject<UWidgetTree>(this);
    }

    if (!WidgetTree->RootWidget)
    {
        UCanvasPanel* Canvas = WidgetTree->ConstructWidget<UCanvasPanel>(
            UCanvasPanel::StaticClass(), TEXT("HUDCanvas"));

        WidgetTree->RootWidget = Canvas;

        UBorder* Background = WidgetTree->ConstructWidget<UBorder>(
            UBorder::StaticClass(), TEXT("HUDBackground"));

        Background->SetBrushColor(FLinearColor(0.01f, 0.015f, 0.025f, 0.94f));
        Background->SetPadding(FMargin(14.0f));

        UVerticalBox* Box = WidgetTree->ConstructWidget<UVerticalBox>(
            UVerticalBox::StaticClass(), TEXT("HUDBox"));

        Background->SetContent(Box);

        ScoreText = WidgetTree->ConstructWidget<UTextBlock>(
            UTextBlock::StaticClass(), TEXT("ScoreText"));

        TimeText = WidgetTree->ConstructWidget<UTextBlock>(
            UTextBlock::StaticClass(), TEXT("TimeText"));

        PelletsText = WidgetTree->ConstructWidget<UTextBlock>(
            UTextBlock::StaticClass(), TEXT("PelletsText"));

        StateText = WidgetTree->ConstructWidget<UTextBlock>(
            UTextBlock::StaticClass(), TEXT("StateText"));

        RestartButton = WidgetTree->ConstructWidget<UButton>(
            UButton::StaticClass(), TEXT("RestartButton"));

        UTextBlock* ButtonText = WidgetTree->ConstructWidget<UTextBlock>(
            UTextBlock::StaticClass(), TEXT("RestartButtonText"));

        ButtonText->SetText(FText::FromString(TEXT("RESTART")));
        RestartButton->SetContent(ButtonText);

        Box->AddChildToVerticalBox(ScoreText);
        Box->AddChildToVerticalBox(TimeText);
        Box->AddChildToVerticalBox(PelletsText);
        Box->AddChildToVerticalBox(StateText);
        Box->AddChildToVerticalBox(RestartButton);

        const FSlateFontInfo Font =
            FCoreStyle::GetDefaultFontStyle(TEXT("Regular"), 22);

        ScoreText->SetFont(Font);
        TimeText->SetFont(Font);
        PelletsText->SetFont(Font);
        StateText->SetFont(Font);
        ButtonText->SetFont(Font);

        const FSlateColor White(FLinearColor::White);

        ScoreText->SetColorAndOpacity(White);
        TimeText->SetColorAndOpacity(White);
        PelletsText->SetColorAndOpacity(White);
        ButtonText->SetColorAndOpacity(White);
        StateText->SetColorAndOpacity(FSlateColor(FLinearColor::Yellow));

        UCanvasPanelSlot* HudCanvasSlot = Canvas->AddChildToCanvas(Background);
        HudCanvasSlot->SetPosition(FVector2D(20.0f, 20.0f));
        HudCanvasSlot->SetAutoSize(true);

        StateText->SetVisibility(ESlateVisibility::Collapsed);
        RestartButton->SetVisibility(ESlateVisibility::Collapsed);
    }

    return Super::RebuildWidget();
}

void UPacmanHUDWidget::NativeConstruct()
{
    Super::NativeConstruct();

    if (RestartButton)
    {
        RestartButton->OnClicked.RemoveDynamic(
            this, &UPacmanHUDWidget::RestartClicked);

        RestartButton->OnClicked.AddDynamic(
            this, &UPacmanHUDWidget::RestartClicked);
    }

    UpdateInterface();
}

void UPacmanHUDWidget::NativeTick(
    const FGeometry& MyGeometry,
    float InDeltaTime)
{
    Super::NativeTick(MyGeometry, InDeltaTime);
    UpdateInterface();
}

void UPacmanHUDWidget::UpdateInterface()
{
    APacmanGameModeBase* GameMode =
        GetWorld()->GetAuthGameMode<APacmanGameModeBase>();

    if (!GameMode)
    {
        return;
    }

    if (ScoreText)
    {
        ScoreText->SetText(FText::FromString(
            FString::Printf(TEXT("SCORE  %d"), GameMode->GetScore())));
    }

    if (TimeText)
    {
        const int32 Seconds = FMath::FloorToInt(GameMode->GetElapsedTime());

        TimeText->SetText(FText::FromString(
            FString::Printf(TEXT("TIME  %02d:%02d"), Seconds / 60, Seconds % 60)));
    }

    if (PelletsText)
    {
        PelletsText->SetText(FText::FromString(
            FString::Printf(TEXT("PILLS  %d"), GameMode->GetPelletsRemaining())));
    }

    const EPacmanGameState State = GameMode->GetGameState();
    const bool bFinished = State != EPacmanGameState::Playing;

    if (StateText)
    {
        StateText->SetVisibility(
            bFinished ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);

        StateText->SetText(FText::FromString(
            State == EPacmanGameState::Won ? TEXT("YOU WIN") : TEXT("GAME OVER")));
    }

    if (RestartButton)
    {
        RestartButton->SetVisibility(
            bFinished ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
    }
}

void UPacmanHUDWidget::RestartClicked()
{
    APacmanGameModeBase* GameMode =
        GetWorld()->GetAuthGameMode<APacmanGameModeBase>();

    if (GameMode)
    {
        GameMode->RestartGame();
    }
}

IMPLEMENT_PRIMARY_GAME_MODULE(FDefaultGameModuleImpl, PacMan, "PacMan");