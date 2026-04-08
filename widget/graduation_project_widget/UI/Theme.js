.pragma library

var background = "#F4F6F8"
var surface = "#FFFFFF"
var surfaceAlt = "#F8FAFC"
var primary = "#006ED8"
var primarySoft = "#E0F2FE"
var teal = "#0D9488"
var text = "#0F172A"
var textSecondary = "#475569"
var textMuted = "#94A3B8"
var border = "#E2E8F0"
var shadow = "#12000000"

var critical = "#EF4444"
var criticalSoft = "#FEE2E2"
var warning = "#F59E0B"
var warningSoft = "#FEF3C7"
var success = "#10B981"
var successSoft = "#D1FAE5"

var sidebarBg = "#1E293B"
var sidebarHover = "#334155"
var sidebarActive = "#0F172A"
var sidebarText = "#CBD5E1"
var monitorDark = "#0B0F19"
var monitorGrid = "#1F2937"

function levelColor(level) {
    if (level === "critical" || level === "error") {
        return critical
    }
    if (level === "warning") {
        return warning
    }
    if (level === "success" || level === "normal" || level === "connected") {
        return success
    }
    if (level === "connecting") {
        return primary
    }
    return textMuted
}

function levelBackground(level) {
    if (level === "critical" || level === "error") {
        return criticalSoft
    }
    if (level === "warning") {
        return warningSoft
    }
    if (level === "success" || level === "normal" || level === "connected") {
        return successSoft
    }
    if (level === "connecting") {
        return primarySoft
    }
    return "#EEF2F7"
}

function metricText(value, suffix) {
    if (suffix === undefined)
        suffix = ""
    return String(value) + suffix
}

function percentText(value) {
    return (value * 100).toFixed(2) + "%"
}
