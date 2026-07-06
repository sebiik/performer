#pragma once

#include "ListModel.h"

#include "engine/generators/SequenceBuilder.h"

class AcidModeSelectListModel : public ListModel {
public:
    void setAllowLayer(bool allowLayer) {
        _allowLayer = allowLayer;
    }

    AcidSequenceBuilder::ApplyMode rowToMode(int row) const {
        if (_allowLayer) {
            if (row == 0) {
                return AcidSequenceBuilder::ApplyMode::Layer;
            }
            return row == 1 ? AcidSequenceBuilder::ApplyMode::Phrase : AcidSequenceBuilder::ApplyMode::EuclideanPhrase;
        }
        return row == 0 ? AcidSequenceBuilder::ApplyMode::Phrase : AcidSequenceBuilder::ApplyMode::EuclideanPhrase;
    }

    virtual int rows() const override {
        return _allowLayer ? 3 : 2;
    }

    virtual int columns() const override {
        return 1;
    }

    virtual void cell(int row, int column, StringBuilder &str) const override {
        if (column != 0) {
            return;
        }

        switch (rowToMode(row)) {
        case AcidSequenceBuilder::ApplyMode::Layer:
            str("Layer");
            break;
        case AcidSequenceBuilder::ApplyMode::Phrase:
            str("Phrase");
            break;
        case AcidSequenceBuilder::ApplyMode::EuclideanPhrase:
            str("Eucl Phrase");
            break;
        }
    }

    virtual void edit(int row, int column, int value, bool shift) override {
        (void)row;
        (void)column;
        (void)value;
        (void)shift;
    }

    virtual void setSelectedScale(int defaultScale, bool force = false) override {
        (void)defaultScale;
        (void)force;
    }

private:
    bool _allowLayer = true;
};
