#include "UnitTest.h"

#include "apps/sequencer/engine/generators/ChaosGenerator.h"
#include "apps/sequencer/engine/generators/EuclideanGenerator.h"
#include "apps/sequencer/engine/generators/RandomGenerator.h"
#include "apps/sequencer/engine/generators/SequenceBuilder.h"
#include "apps/sequencer/model/Project.h"

#include <bitset>
#include <cmath>

namespace {
int noteLayerValue(const NoteSequenceBuilder &builder, int index) {
    const auto range = NoteSequence::layerRange(NoteSequence::Layer::Note);
    return int(std::round(builder.value(index) * (range.max - range.min) + range.min));
}

void selectFirstSteps(std::bitset<CONFIG_STEP_COUNT> &selected, int count) {
    for (int i = 0; i < count; ++i) {
        selected.set(i);
    }
}
}

UNIT_TEST("GeneratorSemantics") {

    CASE("Chaos init preserves seed and scope while resetting generator defaults") {
        Project project;
        project.clear();
        project.setSelectedTrackIndex(0);
        project.setSelectedPatternIndex(0);
        project.setTrackMode(0, Track::TrackMode::Note);

        std::bitset<CONFIG_STEP_COUNT> selected;
        ChaosSequenceBuilder builder(project, selected, ChaosSequenceBuilder::Scope::Pattern);

        ChaosGenerator::Params params;
        params.seed = 0x12345678u;
        params.amount = 37;
        params.targetMask = (1u << int(ChaosGenerator::Target::Gate)) |
                            (1u << int(ChaosGenerator::Target::Slide));
        params.scope = ChaosGenerator::Scope::Pattern;

        ChaosGenerator generator(builder, params, selected);
        generator.init();

        expectEqual(uint32_t(0x12345678u), generator.seed());
        expectTrue(generator.patternScope());
        expectEqual(100, generator.amount());
        expectTrue(generator.allTargetsEnabled());
    }

    CASE("Chaos pattern scope backs up a full Wreck target without heap-backed step buffers") {
        Project project;
        project.clear();
        project.setSelectedTrackIndex(0);
        project.setSelectedPatternIndex(0);

        for (int trackIndex = 0; trackIndex < CONFIG_TRACK_COUNT; ++trackIndex) {
            project.setTrackMode(trackIndex, Track::TrackMode::Note);
            auto &sequence = project.noteSequence(trackIndex, 0);
            sequence.setFirstStep(0);
            sequence.setLastStep(CONFIG_STEP_COUNT - 1);
            for (int stepIndex = 0; stepIndex < CONFIG_STEP_COUNT; ++stepIndex) {
                auto &step = sequence.step(stepIndex);
                step.setGate(true);
                step.setNote(63);
            }
        }

        std::bitset<CONFIG_STEP_COUNT> selected;
        ChaosSequenceBuilder builder(project, selected, ChaosSequenceBuilder::Scope::Pattern);

        expectEqual(CONFIG_TRACK_COUNT, builder.noteTrackCount());
        for (int trackSlot = 0; trackSlot < builder.noteTrackCount(); ++trackSlot) {
            expectEqual(CONFIG_STEP_COUNT, builder.targetStepCount(trackSlot));
        }

        ChaosGenerator::Params params;
        params.seed = 0x13572468u;
        params.amount = 100;
        params.targetMask = (1u << int(ChaosGenerator::Target::Note));
        params.scope = ChaosGenerator::Scope::Pattern;
        params.pivotNote = 0;
        params.span = 48;

        ChaosGenerator generator(builder, params, selected);
        generator.showPreview();

        for (int trackIndex = 0; trackIndex < CONFIG_TRACK_COUNT; ++trackIndex) {
            auto &sequence = project.noteSequence(trackIndex, 0);
            for (int stepIndex = 0; stepIndex < CONFIG_STEP_COUNT; ++stepIndex) {
                expectTrue(sequence.step(stepIndex).note() >= -24);
                expectTrue(sequence.step(stepIndex).note() <= 24);
            }
        }

        generator.showOriginal();

        for (int trackIndex = 0; trackIndex < CONFIG_TRACK_COUNT; ++trackIndex) {
            auto &sequence = project.noteSequence(trackIndex, 0);
            for (int stepIndex = 0; stepIndex < CONFIG_STEP_COUNT; ++stepIndex) {
                expectEqual(63, sequence.step(stepIndex).note());
            }
        }
    }

    CASE("Euclidean randomize params rerolls values while keeping them in valid ranges") {
        NoteSequence sequence;
        sequence.clear();

        NoteSequenceBuilder builder(sequence, NoteSequence::Layer::Gate);

        EuclideanGenerator::Params params;
        params.steps = 16;
        params.beats = 4;
        params.offset = 0;

        EuclideanGenerator generator(builder, params);

        const int initialSteps = generator.steps();
        const int initialBeats = generator.beats();
        const int initialOffset = generator.offset();

        bool changed = false;

        for (int i = 0; i < 8; ++i) {
            generator.randomizeParams();
            generator.update();

            expectTrue(generator.steps() >= 1);
            expectTrue(generator.steps() <= CONFIG_STEP_COUNT);
            expectTrue(generator.beats() >= 1);
            expectTrue(generator.beats() <= generator.steps());
            expectTrue(generator.offset() >= 0);
            expectTrue(generator.offset() < CONFIG_STEP_COUNT);

            if (generator.steps() != initialSteps ||
                generator.beats() != initialBeats ||
                generator.offset() != initialOffset) {
                changed = true;
            }
        }

        expectTrue(changed);
    }

    CASE("Random note layer range stays close to the original register at low range") {
        NoteSequence sequence;
        sequence.clear();
        sequence.setFirstStep(0);
        sequence.setLastStep(15);

        for (int i = 0; i < 16; ++i) {
            auto &step = sequence.step(i);
            step.setGate(true);
            step.setNote(12);
        }

        std::bitset<CONFIG_STEP_COUNT> selected;
        selectFirstSteps(selected, 16);

        NoteSequenceBuilder builder(sequence, NoteSequence::Layer::Note);

        RandomGenerator::Params params;
        params.seed = 0x12345678u;
        params.smooth = 0;
        params.bias = 0;
        params.scale = 5;
        params.variation = 100;

        RandomGenerator generator(builder, params, selected);

        for (int i = 0; i < 16; ++i) {
            const int note = noteLayerValue(builder, i);
            expectTrue(note >= 11);
            expectTrue(note <= 13);
        }
    }

    CASE("Random note layer bias shifts the musical window") {
        NoteSequence sequence;
        sequence.clear();
        sequence.setFirstStep(0);
        sequence.setLastStep(15);

        for (int i = 0; i < 16; ++i) {
            auto &step = sequence.step(i);
            step.setGate(true);
            step.setNote(0);
        }

        std::bitset<CONFIG_STEP_COUNT> selected;
        selectFirstSteps(selected, 16);

        NoteSequenceBuilder builder(sequence, NoteSequence::Layer::Note);

        RandomGenerator::Params params;
        params.seed = 0x87654321u;
        params.smooth = 0;
        params.bias = 3;
        params.scale = 5;
        params.variation = 100;

        RandomGenerator generator(builder, params, selected);

        for (int i = 0; i < 16; ++i) {
            const int note = noteLayerValue(builder, i);
            expectTrue(note >= 8);
            expectTrue(note <= 10);
        }
    }

    CASE("Random generic mapping is unchanged for non-note layers") {
        NoteSequence sequence;
        sequence.clear();

        std::bitset<CONFIG_STEP_COUNT> selected;
        selected.set(0);

        NoteSequenceBuilder builder(sequence, NoteSequence::Layer::Gate);

        const float generated = builder.randomGeneratorValue(0, 0, 0, 5, selected);
        const float expected = 64.f / 255.f;
        expectTrue(std::fabs(generated - expected) < 0.0001f);
    }
}
