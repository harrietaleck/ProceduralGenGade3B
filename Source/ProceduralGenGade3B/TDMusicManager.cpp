#include "TDMusicManager.h"
#include "Engine/Engine.h"
#include "EngineUtils.h"
#include "Kismet/GameplayStatics.h"
#include "TDSaveSubsystem.h"
#include "TimerManager.h"
#include "UObject/ConstructorHelpers.h"

ATDMusicManager::ATDMusicManager()
{
    PrimaryActorTick.bCanEverTick = false;

    MusicAudio = CreateDefaultSubobject<UAudioComponent>(TEXT("MusicAudio"));
    RootComponent = MusicAudio;

    MusicAudio->bAutoActivate = false;
    //  Keep music audible through SetGamePaused and prevent parallel instances.
    MusicAudio->bIsUISound = true;
    MusicAudio->bCanPlayMultipleInstances = false;
    MusicAudio->OnAudioFinished.AddDynamic(this, &ATDMusicManager::HandleMusicFinished);

    static ConstructorHelpers::FObjectFinder<USoundBase> IntroSoundAsset(TEXT("/Game/Audio/IntroSound.IntroSound"));
    static ConstructorHelpers::FObjectFinder<USoundBase> GameplaySoundAsset(TEXT("/Game/Audio/GameplaySound.GameplaySound"));
    static ConstructorHelpers::FObjectFinder<USoundBase> VictorySoundAsset(TEXT("/Game/Audio/VictorySound.VictorySound"));
    static ConstructorHelpers::FObjectFinder<USoundBase> DefeatSoundAsset(TEXT("/Game/Audio/DefeatSound.DefeatSound"));

    if (IntroSoundAsset.Succeeded()) IntroMusic = IntroSoundAsset.Object;
    if (GameplaySoundAsset.Succeeded()) GameplayMusic = GameplaySoundAsset.Object;
    if (VictorySoundAsset.Succeeded()) VictoryMusic = VictorySoundAsset.Object;
    if (DefeatSoundAsset.Succeeded()) DefeatMusic = DefeatSoundAsset.Object;
}

ATDMusicManager* ATDMusicManager::GetOrCreate(const UObject* WorldContextObject)
{
    UWorld* World = WorldContextObject ? WorldContextObject->GetWorld() : nullptr;
    if (!World)
    {
        return nullptr;
    }

    ATDMusicManager* ExistingManager = nullptr;
    for (TActorIterator<ATDMusicManager> It(World); It; ++It)
    {
        ATDMusicManager* Manager = *It;
        if (!IsValid(Manager))
        {
            continue;
        }

        if (!ExistingManager)
        {
            ExistingManager = Manager;
        }
        else
        {
            Manager->Destroy();
        }
    }

    if (ExistingManager)
    {
        return ExistingManager;
    }

    FActorSpawnParameters SpawnParams;
    SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
    return World->SpawnActor<ATDMusicManager>(ATDMusicManager::StaticClass(), FTransform::Identity, SpawnParams);
}

void ATDMusicManager::BeginPlay()
{
    Super::BeginPlay();

    if (GetOrCreate(this) != this)
    {
        return;
    }

    if (UGameInstance* GameInstance = GetGameInstance())
    {
        if (UTDSaveSubsystem* Saves = GameInstance->GetSubsystem<UTDSaveSubsystem>())
        {
            MusicVolume = Saves->GetMusicVolume();
        }
    }

    //  Start the gameplay music automatically.
    const FString LevelName = UGameplayStatics::GetCurrentLevelName(this, true);
    const bool bStartScreen = LevelName.Contains(TEXT("StartScreen"));
    USoundBase* InitialTrack = bStartScreen ? IntroMusic : GameplayMusic;
    if (InitialTrack)
    {
        StartTrack(InitialTrack, bStartScreen);
    }
    else
    {
        UE_LOG(LogTemp, Warning, TEXT("TDMusicManager: Could not load the music asset for level %s."), *LevelName);
    }
}

void ATDMusicManager::SetMusicVolume(float NewVolume)
{
    //  Keep volume within the slider's 0-1 range.
    MusicVolume = FMath::Clamp(NewVolume, 0.0f, 1.0f);

    if (MusicAudio)
    {
        MusicAudio->SetVolumeMultiplier(MusicVolume);
    }

    // Debounce disk writes while dragging; keyboard and controller changes persist too.
    if (UWorld* World = GetWorld())
    {
        World->GetTimerManager().SetTimer(
            VolumeSaveTimer, this, &ATDMusicManager::PersistMusicVolume, 0.4f, false);
    }
}

float ATDMusicManager::GetMusicVolume() const
{
    return MusicVolume;
}

void ATDMusicManager::PlayMusic()
{
    if (MusicAudio && !MusicAudio->IsPlaying())
    {
        MusicAudio->Play();
    }
}

void ATDMusicManager::PlayTrack(USoundBase* NewTrack)
{
    //Change the currently playing track.
    StartTrack(NewTrack, false);
}

void ATDMusicManager::StartTrack(USoundBase* NewTrack, bool bLoop)
{
    if (!MusicAudio || !NewTrack)
    {
        return;
    }

    bLoopCurrentTrack = false;
    MusicAudio->Stop();
    MusicAudio->SetSound(NewTrack);
    MusicAudio->SetVolumeMultiplier(MusicVolume);
    bLoopCurrentTrack = bLoop;
    MusicAudio->Play();
}

void ATDMusicManager::StopMusic()
{
    bLoopCurrentTrack = false;
    if (MusicAudio)
    {
        MusicAudio->Stop();
    }
}

void ATDMusicManager::PlayIntroMusic()
{
    StartTrack(IntroMusic, true);
}

void ATDMusicManager::PlayGameplayMusic()
{
    if (MusicAudio && MusicAudio->Sound == GameplayMusic)
    {
        bLoopCurrentTrack = true;
        if (!MusicAudio->IsPlaying()) PlayMusic();
        return;
    }
    StartTrack(GameplayMusic, true);
}

void ATDMusicManager::PlayVictoryMusic()
{
    StartTrack(VictoryMusic, false);
}

void ATDMusicManager::PlayDefeatMusic()
{
    StartTrack(DefeatMusic, false);
}

// Persist the selected shared value through the existing save subsystem.
void ATDMusicManager::PersistMusicVolume()
{
    if (UWorld* World = GetWorld())
    {
        World->GetTimerManager().ClearTimer(VolumeSaveTimer);
    }

    if (UGameInstance* GameInstance = GetGameInstance())
    {
        if (UTDSaveSubsystem* Saves = GameInstance->GetSubsystem<UTDSaveSubsystem>())
        {
            Saves->SaveMusicVolume(MusicVolume);
        }
    }
}

// Persist the selected shared value through the existing save subsystem.
void ATDMusicManager::HandleMusicFinished()
{
    if (bLoopCurrentTrack && MusicAudio && MusicAudio->Sound && !IsActorBeingDestroyed())
    {
        MusicAudio->Play();
    }
}