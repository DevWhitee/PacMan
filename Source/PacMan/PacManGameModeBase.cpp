#include "PacManGameModeBase.h"

#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "Components/StaticMeshComponent.h"

#include "Engine/StaticMesh.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/World.h"

#include "GameFramework/Controller.h"
#include "GameFramework/PlayerController.h"

#include "Kismet/GameplayStatics.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "TimerManager.h"

APacmanGameModeBase::APacmanGameModeBase()
{
    PrimaryActorTick.bCanEverTick = true;

    DefaultPawnClass = APacmanPlayer::StaticClass();
    PlayerControllerClass = APlayerController::StaticClass();

    Player = nullptr;
    HUD = nullptr;
    GameCamera = nullptr;

    MapRows = 15;
    MapColumns = 17;
    CellSize = 200.0f;

    Score = 0;
    TotalPellets = 0;
    CollectedPellets = 0;

    StartTime = 0.0f;
    EndTime = 0.0f;

    bRestarting = true;
    CurrentState = EPacmanGameState::Playing;
}

void APacmanGameModeBase::BeginPlay()
{
    Super::BeginPlay();

    BuildRound();
    SpawnHUD();
}

void APacmanGameModeBase::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);
    SetupGameCamera();
}

bool APacmanGameModeBase::IsRoundActive() const
{
    return !bRestarting && CurrentState == EPacmanGameState::Playing;
}

APacmanPlayer* APacmanGameModeBase::GetPacmanPlayer() const
{
    return IsValid(Player) ? Player : nullptr;
}

void APacmanGameModeBase::BuildRound()
{
    bRestarting = true;

    APlayerController* PC =
        UGameplayStatics::GetPlayerController(GetWorld(), 0);

    if (!IsValid(Player) && PC)
    {
        Player = Cast<APacmanPlayer>(PC->GetPawn());
    }

    if (IsValid(Player))
    {
        Player->StopPlayer();
        Player->SetActorEnableCollision(false);
    }

    GenerateMaze();
    BuildMap();

    const FVector PlayerLocation =
        CellToWorld(1, 1) + FVector(0.0f, 0.0f, 42.0f);

    if (!IsValid(Player))
    {
        FActorSpawnParameters SpawnParams;
        SpawnParams.SpawnCollisionHandlingOverride =
            ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

        Player = GetWorld()->SpawnActor<APacmanPlayer>(
            APacmanPlayer::StaticClass(),
            PlayerLocation,
            FRotator::ZeroRotator,
            SpawnParams);
    }

    if (IsValid(Player))
    {
        Player->SetActorEnableCollision(false);
        Player->ResetPlayer(PlayerLocation);
    }

    Score = 0;
    CollectedPellets = 0;
    TotalPellets = 0;

    SpawnPellets();
    SpawnGhosts();

    StartTime = GetWorld()->GetTimeSeconds();
    EndTime = 0.0f;
    CurrentState = EPacmanGameState::Playing;

    if (PC)
    {
        PC->bAutoManageActiveCameraTarget = false;

        if (IsValid(Player))
        {
            if (PC->GetPawn() != Player)
            {
                PC->Possess(Player);
            }

            Player->EnableInput(PC);
        }

        PC->SetIgnoreMoveInput(false);

        FInputModeGameOnly InputMode;
        PC->SetInputMode(InputMode);
        PC->bShowMouseCursor = false;
    }

    SetupGameCamera();
    bRestarting = false;

    if (IsValid(Player))
    {
        Player->SetActorEnableCollision(true);
        Player->SetActorTickEnabled(true);
    }

    UE_LOG(LogTemp, Log, TEXT("Pacman round created. Pellets = %d"), TotalPellets);
}

void APacmanGameModeBase::GenerateMaze()
{
    MapRows = FMath::Max(7, MapRows | 1);
    MapColumns = FMath::Max(7, MapColumns | 1);

    MapLayout.Empty();

    for (int32 Row = 0; Row < MapRows; ++Row)
    {
        FString Line;

        for (int32 Column = 0; Column < MapColumns; ++Column)
        {
            Line.AppendChar(TEXT('#'));
        }

        MapLayout.Add(Line);
    }

    const FIntPoint Directions[] =
    {
        FIntPoint(1, 0),
        FIntPoint(-1, 0),
        FIntPoint(0, 1),
        FIntPoint(0, -1)
    };

    TArray<FIntPoint> Stack;

    MapLayout[1][1] = TEXT(' ');
    Stack.Add(FIntPoint(1, 1));

    while (Stack.Num() > 0)
    {
        const FIntPoint Current = Stack.Last();
        TArray<FIntPoint> Options;

        for (const FIntPoint& Direction : Directions)
        {
            const FIntPoint Next = Current + Direction * 2;

            if (Next.X > 0 &&
                Next.X < MapColumns - 1 &&
                Next.Y > 0 &&
                Next.Y < MapRows - 1 &&
                MapLayout[Next.Y][Next.X] == TEXT('#'))
            {
                Options.Add(Direction);
            }
        }

        if (Options.Num() == 0)
        {
            Stack.Pop();
            continue;
        }

        const FIntPoint Direction =
            Options[FMath::RandRange(0, Options.Num() - 1)];

        const FIntPoint Middle = Current + Direction;
        const FIntPoint Next = Current + Direction * 2;

        MapLayout[Middle.Y][Middle.X] = TEXT(' ');
        MapLayout[Next.Y][Next.X] = TEXT(' ');

        Stack.Add(Next);
    }

    for (int32 Row = 1; Row < MapRows - 1; Row += 2)
    {
        for (int32 Column = 1; Column < MapColumns - 1; Column += 2)
        {
            int32 OpenNeighbours = 0;

            for (const FIntPoint& Direction : Directions)
            {
                if (MapLayout[Row + Direction.Y][Column + Direction.X] != TEXT('#'))
                {
                    ++OpenNeighbours;
                }
            }

            if (OpenNeighbours >= 2)
            {
                continue;
            }

            TArray<FIntPoint> Connections;

            for (const FIntPoint& Direction : Directions)
            {
                const int32 TargetRow = Row + Direction.Y * 2;
                const int32 TargetColumn = Column + Direction.X * 2;

                if (TargetRow <= 0 ||
                    TargetRow >= MapRows - 1 ||
                    TargetColumn <= 0 ||
                    TargetColumn >= MapColumns - 1)
                {
                    continue;
                }

                if (MapLayout[Row + Direction.Y][Column + Direction.X] == TEXT('#'))
                {
                    Connections.Add(Direction);
                }
            }

            if (Connections.Num() > 0)
            {
                const FIntPoint Direction =
                    Connections[FMath::RandRange(0, Connections.Num() - 1)];

                MapLayout[Row + Direction.Y][Column + Direction.X] = TEXT(' ');
            }
        }
    }
}

void APacmanGameModeBase::BuildMap()
{
    SpawnBlock(
        FVector(0.0f, 0.0f, -10.0f),
        FVector(
            MapColumns * CellSize / 100.0f,
            MapRows * CellSize / 100.0f,
            0.2f),
        FLinearColor(0.018f, 0.025f, 0.045f, 1.0f));

    const float WallHeight = 180.0f;

    for (int32 Row = 0; Row < MapRows; ++Row)
    {
        for (int32 Column = 0; Column < MapColumns; ++Column)
        {
            if (!IsWalkable(Row, Column))
            {
                SpawnBlock(
                    CellToWorld(Row, Column) +
                        FVector(0.0f, 0.0f, WallHeight * 0.5f),
                    FVector(
                        CellSize / 100.0f,
                        CellSize / 100.0f,
                        WallHeight / 100.0f),
                    FLinearColor(0.035f, 0.2f, 0.7f, 1.0f));
            }
        }
    }
}

void APacmanGameModeBase::SpawnBlock(
    const FVector& Location,
    const FVector& Scale,
    const FLinearColor& Color)
{
    UStaticMesh* CubeMesh = LoadObject<UStaticMesh>(
        nullptr, TEXT("/Engine/BasicShapes/Cube.Cube"));

    if (!CubeMesh)
    {
        UE_LOG(LogTemp, Error, TEXT("Pacman cube mesh not found"));
        return;
    }

    FActorSpawnParameters SpawnParams;
    SpawnParams.SpawnCollisionHandlingOverride =
        ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

    AStaticMeshActor* Block = GetWorld()->SpawnActor<AStaticMeshActor>(
        AStaticMeshActor::StaticClass(),
        Location,
        FRotator::ZeroRotator,
        SpawnParams);

    if (!Block)
    {
        return;
    }

    UStaticMeshComponent* BlockMesh = Block->GetStaticMeshComponent();

    BlockMesh->SetMobility(EComponentMobility::Movable);
    BlockMesh->SetStaticMesh(CubeMesh);

    Block->SetActorScale3D(Scale);

    BlockMesh->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
    BlockMesh->SetCollisionObjectType(ECC_WorldStatic);
    BlockMesh->SetCollisionResponseToAllChannels(ECR_Block);
    BlockMesh->SetCanEverAffectNavigation(true);

    UMaterialInterface* BaseMaterial = LoadObject<UMaterialInterface>(
        nullptr, TEXT("/Game/Materials/M_PacmanColor.M_PacmanColor"));

    if (BaseMaterial)
    {
        BlockMesh->SetMaterial(0, BaseMaterial);

        UMaterialInstanceDynamic* Material =
            BlockMesh->CreateAndSetMaterialInstanceDynamic(0);

        if (Material)
        {
            Material->SetVectorParameterValue(TEXT("Color"), Color);
        }
    }

    GeneratedActors.Add(Block);
}

void APacmanGameModeBase::SpawnPellets()
{
    TotalPellets = 0;

    const FIntPoint Starts[] =
    {
        FIntPoint(1, 1),
        FIntPoint(MapColumns - 2, 1),
        FIntPoint(1, MapRows - 2),
        FIntPoint(MapColumns - 2, MapRows - 2)
    };

    FActorSpawnParameters SpawnParams;
    SpawnParams.SpawnCollisionHandlingOverride =
        ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

    for (int32 Row = 1; Row < MapRows - 1; ++Row)
    {
        for (int32 Column = 1; Column < MapColumns - 1; ++Column)
        {
            if (!IsWalkable(Row, Column))
            {
                continue;
            }

            bool bStartCell = false;

            for (const FIntPoint& StartCell : Starts)
            {
                if (StartCell.X == Column && StartCell.Y == Row)
                {
                    bStartCell = true;
                    break;
                }
            }

            if (bStartCell)
            {
                continue;
            }

            APacmanPellet* Pellet = GetWorld()->SpawnActor<APacmanPellet>(
                APacmanPellet::StaticClass(),
                CellToWorld(Row, Column) + FVector(0.0f, 0.0f, 22.0f),
                FRotator::ZeroRotator,
                SpawnParams);

            if (Pellet)
            {
                GeneratedActors.Add(Pellet);
                ++TotalPellets;
            }
        }
    }
}

void APacmanGameModeBase::SpawnGhosts()
{
    Ghosts.Empty();

    const FIntPoint Starts[] =
    {
        FIntPoint(MapColumns - 2, 1),
        FIntPoint(1, MapRows - 2),
        FIntPoint(MapColumns - 2, MapRows - 2)
    };

    const FLinearColor Colors[] =
    {
        FLinearColor(1.0f, 0.04f, 0.04f, 1.0f),
        FLinearColor(0.05f, 0.85f, 1.0f, 1.0f),
        FLinearColor(0.15f, 1.0f, 0.1f, 1.0f)
    };

    const EPacmanGhostPattern Patterns[] =
    {
        EPacmanGhostPattern::Chase,
        EPacmanGhostPattern::Ambush,
        EPacmanGhostPattern::Flank
    };

    FActorSpawnParameters SpawnParams;
    SpawnParams.SpawnCollisionHandlingOverride =
        ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

    for (int32 Index = 0; Index < 3; ++Index)
    {
        const FVector Location =
            CellToWorld(Starts[Index].Y, Starts[Index].X) +
            FVector(0.0f, 0.0f, 50.0f);

        APacmanGhost* Ghost = GetWorld()->SpawnActor<APacmanGhost>(
            APacmanGhost::StaticClass(),
            Location,
            FRotator::ZeroRotator,
            SpawnParams);

        if (!Ghost)
        {
            continue;
        }

        Ghost->ConfigureGhost(Index, Patterns[Index], Colors[Index]);

        if (!Ghost->GetController())
        {
            Ghost->SpawnDefaultController();
        }

        Ghosts.Add(Ghost);
        GeneratedActors.Add(Ghost);
    }
}

void APacmanGameModeBase::ClearGeneratedActors()
{
    for (APacmanGhost* Ghost : Ghosts)
    {
        if (!IsValid(Ghost))
        {
            continue;
        }

        Ghost->StopGhost();

        AController* GhostController = Ghost->GetController();

        if (IsValid(GhostController))
        {
            GhostController->UnPossess();
            GhostController->Destroy();
        }
    }

    Ghosts.Empty();

    for (AActor* GeneratedActor : GeneratedActors)
    {
        if (IsValid(GeneratedActor))
        {
            GeneratedActor->SetActorEnableCollision(false);
            GeneratedActor->SetActorHiddenInGame(true);
            GeneratedActor->Destroy();
        }
    }

    GeneratedActors.Empty();
}

void APacmanGameModeBase::RestartGame()
{
    if (bRestarting)
    {
        return;
    }

    bRestarting = true;

    if (IsValid(Player))
    {
        Player->StopPlayer();
        Player->SetActorEnableCollision(false);
    }

    ClearGeneratedActors();

    GetWorldTimerManager().SetTimerForNextTick(
        this, &APacmanGameModeBase::BuildRound);
}

void APacmanGameModeBase::CollectPellet()
{
    if (!IsRoundActive())
    {
        return;
    }

    ++CollectedPellets;
    Score += 10;

    if (TotalPellets > 0 && CollectedPellets >= TotalPellets)
    {
        SetGameState(EPacmanGameState::Won);
    }
}

void APacmanGameModeBase::LoseGame()
{
    if (IsRoundActive())
    {
        SetGameState(EPacmanGameState::Lost);
    }
}

void APacmanGameModeBase::SetGameState(EPacmanGameState NewState)
{
    if (!IsRoundActive() || NewState == EPacmanGameState::Playing)
    {
        return;
    }

    CurrentState = NewState;
    EndTime = GetWorld()->GetTimeSeconds();

    if (IsValid(Player))
    {
        Player->StopPlayer();
    }

    for (APacmanGhost* Ghost : Ghosts)
    {
        if (IsValid(Ghost))
        {
            Ghost->StopGhost();
        }
    }

    APlayerController* PC =
        UGameplayStatics::GetPlayerController(GetWorld(), 0);

    if (PC)
    {
        FInputModeGameAndUI InputMode;
        InputMode.SetHideCursorDuringCapture(false);

        PC->SetInputMode(InputMode);
        PC->bShowMouseCursor = true;
    }
}

FVector APacmanGameModeBase::GetGhostTarget(
    EPacmanGhostPattern Pattern,
    const FVector& GhostLocation) const
{
    if (!IsValid(Player))
    {
        return GhostLocation;
    }

    const FIntPoint PlayerCell = WorldToCell(Player->GetActorLocation());
    FIntPoint DesiredCell = PlayerCell;

    FVector PlayerDirection = Player->GetVelocity().GetSafeNormal2D();
    FIntPoint GridDirection(0, 0);

    if (!PlayerDirection.IsNearlyZero())
    {
        if (FMath::Abs(PlayerDirection.X) >= FMath::Abs(PlayerDirection.Y))
        {
            GridDirection.X = PlayerDirection.X >= 0.0f ? 1 : -1;
        }
        else
        {
            GridDirection.Y = PlayerDirection.Y >= 0.0f ? 1 : -1;
        }
    }

    if (Pattern == EPacmanGhostPattern::Ambush)
    {
        if (FVector::Dist2D(GhostLocation, Player->GetActorLocation()) >
            CellSize * 2.0f)
        {
            DesiredCell = PlayerCell + GridDirection * 2;
        }
    }
    else if (Pattern == EPacmanGhostPattern::Flank)
    {
        const int32 Phase = FMath::FloorToInt(GetElapsedTime() / 4.0f) % 2;

        if (Phase == 1 &&
            FVector::Dist2D(GhostLocation, Player->GetActorLocation()) >
                CellSize * 2.0f)
        {
            FIntPoint SideDirection(-GridDirection.Y, GridDirection.X);

            if (SideDirection == FIntPoint::ZeroValue)
            {
                SideDirection = FIntPoint(1, 0);
            }

            DesiredCell = PlayerCell + SideDirection * 2;
        }
    }

    DesiredCell.X = FMath::Clamp(DesiredCell.X, 1, MapColumns - 2);
    DesiredCell.Y = FMath::Clamp(DesiredCell.Y, 1, MapRows - 2);

    const FIntPoint TargetCell = FindNearestWalkable(DesiredCell);
    return CellToWorld(TargetCell.Y, TargetCell.X);
}

FIntPoint APacmanGameModeBase::FindNearestWalkable(
    const FIntPoint& DesiredCell) const
{
    FIntPoint BestCell(1, 1);
    int32 BestDistance = MAX_int32;

    for (int32 Row = 1; Row < MapRows - 1; ++Row)
    {
        for (int32 Column = 1; Column < MapColumns - 1; ++Column)
        {
            if (!IsWalkable(Row, Column))
            {
                continue;
            }

            const int32 Distance =
                FMath::Abs(Column - DesiredCell.X) +
                FMath::Abs(Row - DesiredCell.Y);

            if (Distance < BestDistance)
            {
                BestDistance = Distance;
                BestCell = FIntPoint(Column, Row);
            }
        }
    }

    return BestCell;
}

bool APacmanGameModeBase::IsWalkable(int32 Row, int32 Column) const
{
    return MapLayout.IsValidIndex(Row) &&
        Column >= 0 &&
        Column < MapLayout[Row].Len() &&
        MapLayout[Row][Column] != TEXT('#');
}

FVector APacmanGameModeBase::CellToWorld(int32 Row, int32 Column) const
{
    return FVector(
        (Column - (MapColumns - 1) * 0.5f) * CellSize,
        (Row - (MapRows - 1) * 0.5f) * CellSize,
        0.0f);
}

FIntPoint APacmanGameModeBase::WorldToCell(const FVector& Location) const
{
    return FIntPoint(
        FMath::Clamp(
            FMath::RoundToInt(
                Location.X / CellSize + (MapColumns - 1) * 0.5f),
            0,
            MapColumns - 1),
        FMath::Clamp(
            FMath::RoundToInt(
                Location.Y / CellSize + (MapRows - 1) * 0.5f),
            0,
            MapRows - 1));
}

void APacmanGameModeBase::SpawnHUD()
{
    if (IsValid(HUD))
    {
        return;
    }

    APlayerController* PC =
        UGameplayStatics::GetPlayerController(GetWorld(), 0);

    if (!PC)
    {
        return;
    }

    HUD = CreateWidget<UPacmanHUDWidget>(
        PC, UPacmanHUDWidget::StaticClass());

    if (HUD)
    {
        HUD->AddToViewport(100);
    }
}

void APacmanGameModeBase::SetupGameCamera()
{
    APlayerController* PC =
        UGameplayStatics::GetPlayerController(GetWorld(), 0);

    if (!PC)
    {
        return;
    }

    if (!IsValid(GameCamera))
    {
        FActorSpawnParameters SpawnParams;
        SpawnParams.SpawnCollisionHandlingOverride =
            ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

        GameCamera = GetWorld()->SpawnActor<ACameraActor>(
            ACameraActor::StaticClass(),
            FVector(0.0f, 0.0f, 5000.0f),
            FRotator(-90.0f, 0.0f, 0.0f),
            SpawnParams);
    }

    if (!IsValid(GameCamera))
    {
        return;
    }

    UCameraComponent* CameraComponent = GameCamera->GetCameraComponent();

    if (CameraComponent)
    {
        int32 ViewportWidth = 0;
        int32 ViewportHeight = 0;

        PC->GetViewportSize(ViewportWidth, ViewportHeight);

        float Aspect = 16.0f / 9.0f;

        if (ViewportWidth > 0 && ViewportHeight > 0)
        {
            Aspect = static_cast<float>(ViewportWidth) /
                static_cast<float>(ViewportHeight);
        }

        const float ScreenMapWidth = MapRows * CellSize;
        const float ScreenMapHeight = MapColumns * CellSize;

        CameraComponent->ProjectionMode = ECameraProjectionMode::Orthographic;
        CameraComponent->OrthoWidth =
            FMath::Max(ScreenMapWidth, ScreenMapHeight * Aspect) * 1.20f;
        CameraComponent->bConstrainAspectRatio = false;
    }

    PC->bAutoManageActiveCameraTarget = false;

    if (PC->GetViewTarget() != GameCamera)
    {
        PC->SetViewTarget(GameCamera);
    }
}

int32 APacmanGameModeBase::GetScore() const
{
    return Score;
}

int32 APacmanGameModeBase::GetPelletsRemaining() const
{
    return FMath::Max(0, TotalPellets - CollectedPellets);
}

float APacmanGameModeBase::GetElapsedTime() const
{
    if (!GetWorld())
    {
        return 0.0f;
    }

    const float CurrentTime = CurrentState == EPacmanGameState::Playing
        ? GetWorld()->GetTimeSeconds()
        : EndTime;

    return FMath::Max(0.0f, CurrentTime - StartTime);
}

FIntPoint APacmanGameModeBase::FindNextCell(
    const FIntPoint& Start,
    const FIntPoint& Goal) const
{
    if (!IsWalkable(Start.Y, Start.X) || !IsWalkable(Goal.Y, Goal.X))
    {
        return Start;
    }

    if (Start == Goal)
    {
        return Goal;
    }

    const int32 CellCount = MapRows * MapColumns;

    TArray<int32> Parents;
    Parents.Init(INDEX_NONE, CellCount);

    TArray<int32> Queue;
    Queue.Reserve(CellCount);

    const int32 StartIndex = Start.Y * MapColumns + Start.X;
    const int32 GoalIndex = Goal.Y * MapColumns + Goal.X;

    Parents[StartIndex] = StartIndex;
    Queue.Add(StartIndex);

    const FIntPoint Directions[] =
    {
        FIntPoint(1, 0),
        FIntPoint(-1, 0),
        FIntPoint(0, 1),
        FIntPoint(0, -1)
    };

    int32 QueuePosition = 0;

    while (QueuePosition < Queue.Num())
    {
        const int32 CurrentIndex = Queue[QueuePosition++];

        if (CurrentIndex == GoalIndex)
        {
            break;
        }

        const FIntPoint Current(
            CurrentIndex % MapColumns,
            CurrentIndex / MapColumns);

        for (const FIntPoint& Direction : Directions)
        {
            const FIntPoint Next = Current + Direction;

            if (!IsWalkable(Next.Y, Next.X))
            {
                continue;
            }

            const int32 NextIndex = Next.Y * MapColumns + Next.X;

            if (Parents[NextIndex] != INDEX_NONE)
            {
                continue;
            }

            Parents[NextIndex] = CurrentIndex;
            Queue.Add(NextIndex);
        }
    }

    if (Parents[GoalIndex] == INDEX_NONE)
    {
        return Start;
    }

    int32 StepIndex = GoalIndex;

    while (Parents[StepIndex] != StartIndex)
    {
        const int32 ParentIndex = Parents[StepIndex];

        if (ParentIndex == INDEX_NONE || ParentIndex == StepIndex)
        {
            return Start;
        }

        StepIndex = ParentIndex;
    }

    return FIntPoint(
        StepIndex % MapColumns,
        StepIndex / MapColumns);
}

FVector APacmanGameModeBase::GetGhostWaypoint(
    EPacmanGhostPattern Pattern,
    const FVector& GhostLocation) const
{
    if (!IsValid(Player) || MapLayout.Num() == 0)
    {
        return GhostLocation;
    }

    const FIntPoint CurrentCell = WorldToCell(GhostLocation);

    if (!IsWalkable(CurrentCell.Y, CurrentCell.X))
    {
        return GhostLocation;
    }

    const FVector DesiredTarget = GetGhostTarget(Pattern, GhostLocation);
    FIntPoint GoalCell = WorldToCell(DesiredTarget);

    if (!IsWalkable(GoalCell.Y, GoalCell.X))
    {
        GoalCell = FindNearestWalkable(GoalCell);
    }

    if (CurrentCell == GoalCell)
    {
        const FIntPoint PlayerCell = WorldToCell(Player->GetActorLocation());

        if (PlayerCell == CurrentCell)
        {
            FVector Target = Player->GetActorLocation();
            Target.Z = GhostLocation.Z;
            return Target;
        }

        FVector Target = CellToWorld(GoalCell.Y, GoalCell.X);
        Target.Z = GhostLocation.Z;
        return Target;
    }

    const FIntPoint NextCell = FindNextCell(CurrentCell, GoalCell);

    FVector CurrentCenter = CellToWorld(CurrentCell.Y, CurrentCell.X);
    CurrentCenter.Z = GhostLocation.Z;

    if (NextCell == CurrentCell)
    {
        return CurrentCenter;
    }

    FVector NextCenter = CellToWorld(NextCell.Y, NextCell.X);
    NextCenter.Z = GhostLocation.Z;

    const bool bMovingAlongX = NextCell.X != CurrentCell.X;

    const float LateralOffset = bMovingAlongX
        ? FMath::Abs(GhostLocation.Y - CurrentCenter.Y)
        : FMath::Abs(GhostLocation.X - CurrentCenter.X);

    if (LateralOffset > 10.0f)
    {
        return CurrentCenter;
    }

    return NextCenter;
}

EPacmanGameState APacmanGameModeBase::GetGameState() const
{
    return CurrentState;
}