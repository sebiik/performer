#include "UnitTest.h"

#include "core/utils/StringBuilder.h"

#include "apps/sequencer/model/Scale.cpp"
#include "apps/sequencer/model/UserScale.cpp"
#include "apps/sequencer/model/UserSettings.h"

#include <array>
#include <cstdint>
#include <cstring>
#include <vector>

UNIT_TEST("Scale") {

    CASE("defensive scale and note lookup") {
        expectEqual(Scale::name(-1), "?");
        expectEqual(Scale::name(Scale::Count), "?");
        expectEqual(Scale::get(-1).notesPerOctave(), Scale::get(0).notesPerOctave());
        expectEqual(Scale::get(Scale::Count).notesPerOctave(), Scale::get(Scale::Count - 1).notesPerOctave());

        FixedStringBuilder<8> note;
        Types::printNote(note, -1);
        expectEqual((const char *)note, "B");
        note.reset();
        Types::printNote(note, 12);
        expectEqual((const char *)note, "C");
    }

    CASE("defensive user setting menu key") {
        ChaosPivotNoteSetting pivot;
        pivot.setValue(13);
        expectEqual(pivot.getMenuItemKey().c_str(), "edit");
    }

    CASE("defensive user scale read") {
        std::vector<uint8_t> data;
        VersionedSerializedWriter writer([&data] (const void *source, size_t len) {
            const auto *bytes = static_cast<const uint8_t *>(source);
            data.insert(data.end(), bytes, bytes + len);
        }, ProjectVersion::Latest);

        char name[UserScale::NameLength + 1];
        std::memset(name, 0, sizeof(name));
        UserScale::Mode mode = UserScale::Mode::Chromatic;
        uint8_t invalidSize = 0;
        writer.write(name, sizeof(name));
        writer.write(mode);
        writer.write(invalidSize);
        writer.writeHash();

        size_t offset = 0;
        VersionedSerializedReader reader([&data, &offset] (void *target, size_t len) {
            std::memcpy(target, data.data() + offset, len);
            offset += len;
        }, ProjectVersion::Latest);

        UserScale scale;
        expectTrue(scale.read(reader));
        expectEqual(scale.size(), 1);
        expectEqual(scale.notesPerOctave(), 1);

        FixedStringBuilder<8> note;
        scale.noteName(note, 0, 0, Scale::Long);
        expectEqual((const char *)note, "C+0");
    }

    CASE("noteName/noteToVolts") {
        for (int i = 0; i < Scale::Count; ++i) {
            const auto &scale = Scale::get(i);
            int notesPerOctave = scale.notesPerOctave();

            DBG("----------------------------------------");
            DBG("%s", Scale::name(i));
            DBG("octave has %d notes", notesPerOctave);
            DBG("----------------------------------------");
            DBG("%-8s %-8s %-8s %-8s %-8s", "note", "volts", "short1", "short2", "long");

            for (int note = -notesPerOctave; note <= notesPerOctave; ++note) {
                FixedStringBuilder<8> short1Name;
                FixedStringBuilder<8> short2Name;
                FixedStringBuilder<16> longName;
                scale.noteName(short1Name, note, 0, Scale::Short1);
                scale.noteName(short2Name, note, 0, Scale::Short2);
                scale.noteName(longName, note, 0, Scale::Long);
                float volts = scale.noteToVolts(note);
                DBG("%-8d %-8.3f %-8s %-8s %-8s", note, volts, (const char *)(short1Name), (const char *)(short2Name), (const char *)(longName));
                // DBG("note = %d, volts = %f", note, volts);
            }
        }
    }

    CASE("noteFromVolts") {
        for (int i = 0; i < Scale::Count; ++i) {
            const auto &scale = Scale::get(i);
            int notesPerOctave = scale.notesPerOctave();

            DBG("----------------------------------------");
            DBG("%s", Scale::name(i));
            DBG("octave has %d notes", notesPerOctave);
            DBG("----------------------------------------");
            DBG("%-8s %-8s %-8s %-8s %-8s %-8s", "voltsin", "note", "volts", "short1", "short2", "long");

            for (int index = -12; index <= 12; ++index) {
                float voltsin = index * (1.f / 12.f);
                int note = scale.noteFromVolts(voltsin);
                FixedStringBuilder<8> short1Name;
                FixedStringBuilder<8> short2Name;
                FixedStringBuilder<16> longName;
                scale.noteName(short1Name, note, 0, Scale::Short1);
                scale.noteName(short2Name, note, 0, Scale::Short2);
                scale.noteName(longName, note, 0, Scale::Long);
                float volts = scale.noteToVolts(note);
                DBG("%-8.3f %-8d %-8.3f %-8s %-8s %-8s", voltsin, note, volts, (const char *)(short1Name), (const char *)(short2Name), (const char *)(longName));
            }

        }
    }

#ifdef PLATFORM_SIM

    CASE("markdown") {
        const int ColsPerRow = 8;

        DBG("----------------------------------------");
        for (int i = 0; i < Scale::Count - 4; ++i) {
            const auto &scale = Scale::get(i);
            int notesPerOctave = scale.notesPerOctave();

            DBG("<h4>%s</h4>", Scale::name(i));
            DBG("");

            for (int page = 0; page < (notesPerOctave + ColsPerRow - 1) / ColsPerRow; ++page) {
                FixedStringBuilder<4096> indices("| Index |");
                FixedStringBuilder<4096> separators("| :--- |");
                FixedStringBuilder<4096> volts("| Volts |");
                int begin = page * ColsPerRow;
                int end = (page + 1) * ColsPerRow;
                if (notesPerOctave < end) end = notesPerOctave;
                for (int note = begin; note < end; ++note) {
                    indices(" %d |", note + 1);
                    separators(" --- |");
                    volts(" %.3f |", scale.noteToVolts(note));
                }
                DBG("%s", (const char *)(indices));
                DBG("%s", (const char *)(separators));
                DBG("%s", (const char *)(volts));
                DBG("");
            }
        }
        DBG("----------------------------------------");
    }

#endif // PLATFORM_SIM

}
