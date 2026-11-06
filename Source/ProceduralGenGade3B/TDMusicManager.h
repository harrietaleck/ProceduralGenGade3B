#pragma once

#include "CoreMinimal.h"
#include "Engine/TimerHandle.h"
#include "GameFramework/Actor.h"
#include "Sound/SoundBase.h"
#include "Components/AudioComponent.h"
#include "TDMusicManager.generated.h"

UCLASS()
class PROCEDURALGENGADE3B_API ATDMusicManager : public AActor
{
    GENERATED_BODY()

public:
    ATDMusicManager();

    virtual void BeginPlay() override;

    UFUNCTION(BlueprintCallable, Category = "Music", meta = (WorldContext = "WorldContextObject"))
    static ATDMusicManager* GetOrCreate(const UObject* WorldContextObject);

    // Change the music volume using the Pause Settings slider.
    UFUNCTION(BlueprintCallable, Category = "Music")
    void SetMusicVolume(float NewVolume);

    // Save the selected slider value when settings input ends.
    UFUNCTION()
    void PersistMusicVolume();

    UFUNCTION(BlueprintPure, Category = "Music")
    float GetMusicVolume() const;

    // Play the selected music track.
    UFUNCTION(BlueprintCallable, Category = "Music")
    void PlayMusic();

    UFUNCTION(BlueprintCallable, Category = "Music")
    void PlayIntroMusic();

    UFUNCTION(BlueprintCallable, Category = "Music")
    void PlayGameplayMusic();

    UFUNCTION(BlueprintCallable, Category = "Music")
    void PlayVictoryMusic();

    UFUNCTION(BlueprintCallable, Category = "Music")
    void PlayDefeatMusic();

    // Play a specific track.
    UFUNCTION(BlueprintCallable, Category = "Music")
    void PlayTrack(USoundBase* NewTrack);

    // Stop the current music.
    UFUNCTION(BlueprintCallable, Category = "Music")
    void StopMusic();

private:

    void StartTrack(USoundBase* NewTrack, bool bLoop);

    // Persist the selected shared value through the existing save subsystem.
    UFUNCTION()
    void HandleMusicFinished();

    bool bLoopCurrentTrack = false;
    FTimerHandle VolumeSaveTimer;

    UPROPERTY(VisibleAnywhere, Category = "Music")
    TObjectPtr<UAudioComponent> MusicAudio;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Music|Tracks", meta = (AllowPrivateAccess = "true"))
    TObjectPtr<USoundBase> IntroMusic;

    //ASSIGN YOUR GAMEPLAY MUSIC HERE IN UNREAL.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Music|Tracks", meta = (AllowPrivateAccess = "true"))
    TObjectPtr<USoundBase> GameplayMusic;

    // ASSIGN YOUR VICTORY MUSIC HERE.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Music|Tracks", meta = (AllowPrivateAccess = "true"))
    TObjectPtr<USoundBase> VictoryMusic;

    //ASSIGN YOUR DEFEAT MUSIC HERE.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Music|Tracks", meta = (AllowPrivateAccess = "true"))
    TObjectPtr<USoundBase> DefeatMusic;

    //ASSIGN YOUR BOSS OR SPECIAL WAVE MUSIC HERE.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Music|Tracks", meta = (AllowPrivateAccess = "true"))
    TObjectPtr<USoundBase> SpecialWaveMusic;

    //Overall music volume: 0 = mute, 1 = maximum.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Music|Settings", meta = (ClampMin = "0.0", ClampMax = "1.0", AllowPrivateAccess = "true"))
    float MusicVolume = 0.5f;
};