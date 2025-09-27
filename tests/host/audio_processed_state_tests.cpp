#include <gtest/gtest.h>
#include <FixedPoints.h>
#include "audio_processed_state.h"

using SensoryBridge::Audio::AudioProcessedState;

TEST(AudioProcessedState, InitializesBuffersToZero) {
    AudioProcessedState state;

    auto* waveform = state.getWaveform();
    auto* waveform_fixed = state.getWaveformFixedPoint();

    for (int i = 0; i < 1024; ++i) {
        EXPECT_EQ(0, waveform[i]);
        EXPECT_EQ(SQ15x16(0), waveform_fixed[i]);
    }

    EXPECT_TRUE(state.validateState());
}

TEST(AudioProcessedState, BeginFrameResetsPeakAndTracksFrameCount) {
    AudioProcessedState state;

    state.updatePeak(0.5f);
    EXPECT_FLOAT_EQ(0.5f, state.getMaxRaw());

    state.beginFrame();
    EXPECT_FLOAT_EQ(0.0f, state.getMaxRaw());
    EXPECT_EQ(1u, state.getFrameCount());

    state.beginFrame();
    EXPECT_EQ(2u, state.getFrameCount());
}

TEST(AudioProcessedState, UpdatePeakTracksMaximumOnly) {
    AudioProcessedState state;

    state.beginFrame();
    state.updatePeak(0.2f);
    state.updatePeak(0.8f);
    state.updatePeak(0.4f); // lower than current max

    EXPECT_FLOAT_EQ(0.8f, state.getMaxRaw());
}

TEST(AudioProcessedState, UpdateVolumeAnalysisPersistsValues) {
    AudioProcessedState state;

    state.updateVolumeAnalysis(1.2f, 0.9f, 0.75f);

    EXPECT_FLOAT_EQ(1.2f, state.getMax());
    EXPECT_FLOAT_EQ(0.9f, state.getMaxFollower());
    EXPECT_FLOAT_EQ(0.75f, state.getPeakScaled());
}

TEST(AudioProcessedState, SilentStateAndPunchAccessorsWork) {
    AudioProcessedState state;

    state.setSilent(true);
    state.setSilentScale(0.42f);
    state.setCurrentPunch(0.33f);

    EXPECT_TRUE(state.isSilent());
    EXPECT_FLOAT_EQ(0.42f, state.getSilentScale());
    EXPECT_FLOAT_EQ(0.33f, state.getCurrentPunch());
}
