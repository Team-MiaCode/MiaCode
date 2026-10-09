package org.miacode.android;

import android.app.*;
import android.content.*;
import android.content.pm.ServiceInfo;
import android.os.*;
import org.json.JSONObject;

// Keeps the local Qt render/audio/codec process alive only when the user has
// enabled background export. Cancellation is queued onto the Qt job owner.
public final class ChartExportService extends Service {
    private static final String CHANNEL = "miacode-chart-export";
    private static final int NOTIFICATION = 2402;
    private static volatile ChartExportService current;
    private PowerManager.WakeLock wakeLock;
    private int percent;
    private long lastProgressNotification;

    public static void begin(Context context) {
        context.startForegroundService(new Intent(context, ChartExportService.class));
    }
    public static void end(Context context) {
        context.stopService(new Intent(context, ChartExportService.class));
    }
    public static void progress(int percent) {
        ChartExportService service = current;
        if (service != null) service.getMainExecutor().execute(() -> {
            int next = Math.max(0, Math.min(100, percent));
            if (current != service || service.percent == next) return;
            service.percent = next;
            long now = SystemClock.uptimeMillis();
            // Rendering polls frequently; keep the system shade responsive.
            if (next < 100 && now - service.lastProgressNotification < 1500) return;
            service.lastProgressNotification = now;
            service.getSystemService(NotificationManager.class).notify(NOTIFICATION, service.notification());
        });
    }
    private Notification notification() {
        Intent open = new Intent(this, MiaCodeActivity.class).addFlags(Intent.FLAG_ACTIVITY_SINGLE_TOP);
        PendingIntent content = PendingIntent.getActivity(this, 0, open, PendingIntent.FLAG_IMMUTABLE | PendingIntent.FLAG_UPDATE_CURRENT);
        Intent cancel = new Intent(this, ChartExportService.class).setAction("cancel");
        PendingIntent stop = PendingIntent.getService(this, 1, cancel, PendingIntent.FLAG_IMMUTABLE | PendingIntent.FLAG_UPDATE_CURRENT);
        return new Notification.Builder(this, CHANNEL).setSmallIcon(android.R.drawable.stat_sys_upload)
            .setContentTitle("MiaCode 谱面导出").setContentText("正在本机离线导出")
            .setOnlyAlertOnce(true).setShowWhen(false)
            .setProgress(100, percent, percent == 0).setOngoing(true).setContentIntent(content)
            .addAction(new Notification.Action.Builder(null, "取消", stop).build()).build();
    }
    @Override public void onCreate() {
        super.onCreate(); current = this;
        lastProgressNotification = SystemClock.uptimeMillis();
        NotificationManager notifications = getSystemService(NotificationManager.class);
        notifications.createNotificationChannel(new NotificationChannel(CHANNEL, "谱面导出", NotificationManager.IMPORTANCE_LOW));
        int type = Build.VERSION.SDK_INT >= 35 ? ServiceInfo.FOREGROUND_SERVICE_TYPE_MEDIA_PROCESSING
            : ServiceInfo.FOREGROUND_SERVICE_TYPE_DATA_SYNC;
        startForeground(NOTIFICATION, notification(), type);
        wakeLock = getSystemService(PowerManager.class).newWakeLock(PowerManager.PARTIAL_WAKE_LOCK, "MiaCode:chartExport");
        wakeLock.acquire(6L * 60 * 60 * 1000);
    }
    private void cancelJob() {
        try { MiaCodeActivity.deliverResult(new JSONObject().put("kind", "chartExportCancel").toString()); }
        catch (Exception ignored) {}
    }
    @Override public int onStartCommand(Intent intent, int flags, int startId) {
        if (intent != null && "cancel".equals(intent.getAction())) cancelJob();
        return START_NOT_STICKY;
    }
    @Override public void onTimeout(int startId, int type) { cancelJob(); stopSelf(); }
    @Override public void onDestroy() {
        current = null;
        if (wakeLock != null && wakeLock.isHeld()) wakeLock.release();
        stopForeground(STOP_FOREGROUND_REMOVE); super.onDestroy();
    }
    @Override public IBinder onBind(Intent intent) { return null; }
}
