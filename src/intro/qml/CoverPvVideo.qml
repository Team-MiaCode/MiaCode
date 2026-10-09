// Silent PV playback for the cover editor's active chart frame (frameBgMode
// "pv" / "pvFit"). While the chart frame plays, the PV plays here at chart time
// (chart second == PV second) over the still; there is no AudioOutput, so it is
// video only. Paused, it hides and the composer's still shows instead.
//
// Loaded by CoverComposer only in edit mode, so the export render never pulls
// in QtMultimedia.

import QtQuick
import QtMultimedia

Item {
    id: root

    property url source: ""
    property real seconds: 0          // the chart frame's playhead
    property bool playing: false      // the chart frame is playing
    // The composer's PV still; a new revision after pausing is the still for
    // the pause time, which may replace the last video frame.
    property int stillRevision: -1

    // Keep showing the last video frame after a pause until the still for that
    // time has arrived, so pausing never flashes back to the pre-play frame.
    property bool holding: false
    property int revisionAtPause: -1
    readonly property bool inSync: Math.abs(player.position - root.seconds * 1000) < 300
    visible: (root.playing && player.playbackState === MediaPlayer.PlayingState && root.inSync)
             || root.holding

    function follow() {
        if (player.mediaStatus === MediaPlayer.NoMedia || player.mediaStatus === MediaPlayer.LoadingMedia)
            return
        if (!root.playing) {
            player.pause()
            return
        }
        // Re-seek only on real drift; seeking every tick would stall decoding.
        if (Math.abs(player.position - root.seconds * 1000) > 250)
            player.position = Math.round(root.seconds * 1000)
        if (player.playbackState !== MediaPlayer.PlayingState)
            player.play()
    }

    onPlayingChanged: {
        if (!root.playing) {
            root.holding = root.visible
            root.revisionAtPause = root.stillRevision
        } else {
            root.holding = false
        }
        root.follow()
    }
    onStillRevisionChanged: {
        if (!root.playing && root.stillRevision !== root.revisionAtPause)
            root.holding = false
    }
    onSecondsChanged: if (root.playing) root.follow()

    VideoOutput {
        id: output
        anchors.fill: parent
        fillMode: VideoOutput.Stretch
    }

    MediaPlayer {
        id: player
        source: root.source
        videoOutput: output
        onMediaStatusChanged: root.follow()
    }
}
