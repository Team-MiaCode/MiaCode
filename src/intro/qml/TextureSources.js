.pragma library

// Android's texture factory keeps local still images in RGBA through upload.
// Other URLs (including chart-frame image providers) retain their own owners.
function source(url, clipRect) {
    var value = url.toString()
    if (Qt.platform.os !== "android" || !value
            || (value.indexOf("qrc:") !== 0 && value.indexOf("file:") !== 0))
        return url
    var result = "image://introasset/" + encodeURIComponent(value)
    if (clipRect && clipRect.width > 0 && clipRect.height > 0)
        result += "?clip=" + [clipRect.x, clipRect.y, clipRect.width, clipRect.height].join(",")
    return result
}
