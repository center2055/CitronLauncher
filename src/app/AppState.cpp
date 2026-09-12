#include "app/AppState.h"

#include "core/Text.h"

namespace citron {

const VersionRow* AppState::selectedRow() const {
    return selected ? find(*selected) : nullptr;
}

const VersionRow* AppState::find(const VersionId& id) const {
    for (const auto& row : versions) {
        if (row.id == id) {
            return &row;
        }
    }
    return nullptr;
}

std::vector<const VersionRow*> AppState::installedRows() const {
    std::vector<const VersionRow*> out;
    for (const auto& row : versions) {
        if (row.installed) {
            out.push_back(&row);
        }
    }
    return out;
}

std::vector<const VersionRow*> AppState::visibleRows() const {
    std::vector<const VersionRow*> out;
    const std::string query = text::lower(text::trim(text::toUtf8(search)));
    for (const auto& row : versions) {
        switch (filter) {
        case VersionFilter::Release:
            if (row.id.channel != VersionChannel::Release) {
                continue;
            }
            break;
        case VersionFilter::Preview:
            if (row.id.channel != VersionChannel::Preview) {
                continue;
            }
            break;
        case VersionFilter::Installed:
            if (!row.installed) {
                continue;
            }
            break;
        case VersionFilter::All:
            break;
        }
        if (!query.empty() && row.id.number.toString().find(query) == std::string::npos) {
            continue;
        }
        out.push_back(&row);
    }
    return out;
}

std::vector<const VersionRow*> AppState::activeOperations() const {
    std::vector<const VersionRow*> out;
    for (const auto& row : versions) {
        if (isBusy(row.progress.stage)) {
            out.push_back(&row);
        }
    }
    return out;
}

std::vector<VersionRow> buildRows(const ServiceSnapshot& snapshot, const std::optional<VersionId>& selected) {
    std::vector<VersionRow> rows;
    rows.reserve(snapshot.versions.size());
    for (const auto& info : snapshot.versions) {
        VersionRow row;
        row.id = info.id;
        row.size = info.size();
        row.installed = info.installed();
        row.downloaded = info.packageFile.has_value() && !info.installed();
        row.deployed = info.deployed;
        row.inCatalog = info.catalog.has_value();
        row.partialSize = info.partialSize;
        row.selected = selected && *selected == info.id;
        if (auto it = snapshot.operations.find(info.id); it != snapshot.operations.end()) {
            row.progress = it->second;
        }
        rows.push_back(std::move(row));
    }
    return rows;
}

std::optional<VersionId> chooseSelection(const std::vector<VersionRow>& rows, const std::optional<VersionId>& previous) {
    if (previous) {
        for (const auto& row : rows) {
            if (row.id == *previous && row.installed) {
                return previous;
            }
        }
    }
    for (const auto& row : rows) {
        if (row.deployed && row.id.channel == VersionChannel::Release) {
            return row.id;
        }
    }
    for (const auto& row : rows) {
        if (row.installed && row.id.channel == VersionChannel::Release) {
            return row.id;
        }
    }
    for (const auto& row : rows) {
        if (row.installed) {
            return row.id;
        }
    }
    return std::nullopt;
}

}
