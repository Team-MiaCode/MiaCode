package org.miacode.android;

import android.content.Context;
import android.database.Cursor;
import android.net.Uri;
import android.provider.DocumentsContract;
import org.json.JSONObject;
import java.io.File;
import java.io.FileInputStream;
import java.io.InputStream;
import java.io.OutputStream;
import java.security.MessageDigest;
import java.util.Locale;
import java.util.HashSet;
import java.util.Set;
import java.util.concurrent.ConcurrentHashMap;
import java.util.concurrent.ExecutorService;
import java.util.concurrent.Executors;
import java.util.concurrent.atomic.AtomicBoolean;

public final class ExportFilePublisher {
    private static final ExecutorService io = Executors.newSingleThreadExecutor();
    private static final ConcurrentHashMap<String, AtomicBoolean> jobs = new ConcurrentHashMap<>();
    private ExportFilePublisher() {}

    public static String mimeForName(String name) {
        String lower = name.toLowerCase(Locale.ROOT);
        if (lower.endsWith(".mp4")) return "video/mp4";
        if (lower.endsWith(".wav")) return "audio/wav";
        if (lower.endsWith(".png")) return "image/png";
        if (lower.endsWith(".jpg") || lower.endsWith(".jpeg")) return "image/jpeg";
        if (lower.endsWith(".zip")) return "application/zip";
        return "application/octet-stream";
    }
    public static void cancel(String token) { AtomicBoolean flag = jobs.get(token); if (flag != null) flag.set(true); }
    public static void publish(Context context, String token, String sourcePath, String targetJson) {
        AtomicBoolean cancelled = new AtomicBoolean(false);
        jobs.put(token, cancelled);
        final Context app = context.getApplicationContext();
        io.execute(() -> {
            Uri outputUri = null;
            try {
                JSONObject target = new JSONObject(targetJson);
                File source = new File(sourcePath);
                if (!source.isFile() || source.length() == 0) throw new java.io.IOException("导出暂存文件不存在");
                outputUri = Uri.parse(target.getString("uri"));
                if (!"content".equals(outputUri.getScheme())) throw new java.io.IOException("无效的导出目标");
                checkCancelled(cancelled);
                if (target.optBoolean("tree")) outputUri = resolveTreeTarget(app, outputUri, target.getString("relativePath"));
                MessageDigest written = MessageDigest.getInstance("SHA-256");
                byte[] buffer = new byte[1024 * 1024];
                long count = 0, size = source.length();
                int lastPercent = -10;
                try (InputStream input = new FileInputStream(source); OutputStream output = app.getContentResolver().openOutputStream(outputUri, "wt")) {
                    if (output == null) throw new java.io.IOException("文件提供方未返回写入流");
                    int length;
                    while ((length = input.read(buffer)) != -1) {
                        checkCancelled(cancelled); output.write(buffer, 0, length); written.update(buffer, 0, length); count += length;
                        int percent = (int)(count * 80 / size);
                        if (percent - lastPercent >= 8) { send(token, false, false, percent, outputUri, "", count, ""); lastPercent = percent; }
                    }
                    output.flush();
                }
                if (count != size) throw new java.io.IOException("导出暂存文件在复制时发生变化");
                // SAF providers do not promise atomic replacement. Keep the complete
                // private file and verify the provider's bytes before reporting success.
                MessageDigest read = MessageDigest.getInstance("SHA-256");
                long checked = 0;
                try (InputStream input = app.getContentResolver().openInputStream(outputUri)) {
                    if (input == null) throw new java.io.IOException("无法读取导出目标进行校验");
                    int length;
                    while ((length = input.read(buffer)) != -1) {
                        checkCancelled(cancelled); checked += length;
                        if (checked > size) throw new java.io.IOException("导出目标大小不一致");
                        read.update(buffer, 0, length);
                    }
                }
                checkCancelled(cancelled);
                byte[] digest = written.digest();
                if (checked != size || !MessageDigest.isEqual(digest, read.digest())) throw new java.io.IOException("导出后的文件校验失败");
                StringBuilder sha = new StringBuilder(); for (byte value : digest) sha.append(String.format(Locale.ROOT, "%02x", value & 255));
                String displayPath = target.optString("displayPath");
                if (target.optBoolean("tree")) {
                    String relative = target.getString("relativePath").replace('\\', '/');
                    int slash = relative.lastIndexOf('/');
                    String parentPath = slash < 0 ? "" : relative.substring(0, slash + 1);
                    String actualName = documentName(app, outputUri);
                    displayPath = (displayPath.isEmpty() ? "" : displayPath + "/") + parentPath + actualName;
                }
                send(token, true, true, 100, outputUri, sha.toString(), count, "", displayPath);
            } catch (Exception error) {
                send(token, true, false, 0, outputUri, "", 0, error.getMessage() == null ? error.toString() : error.getMessage());
            } finally { jobs.remove(token); }
        });
    }
    private static void checkCancelled(AtomicBoolean flag) throws java.io.IOException {
        if (flag.get()) throw new java.io.IOException("导出已取消；完整文件仍保留在应用内");
    }
    private static Uri resolveTreeTarget(Context context, Uri tree, String relative) throws Exception {
        String id = DocumentsContract.getTreeDocumentId(tree);
        String[] parts = relative.replace('\\', '/').split("/");
        Uri parent = DocumentsContract.buildDocumentUriUsingTree(tree, id);
        for (int index = 0; index < parts.length; ++index) {
            String name = parts[index];
            if (name.isEmpty() || name.equals(".") || name.equals("..")) throw new java.io.IOException("无效的导出文件名");
            boolean folder = index < parts.length - 1;
            Uri children = DocumentsContract.buildChildDocumentsUriUsingTree(tree, DocumentsContract.getDocumentId(parent));
            Uri child = null;
            Set<String> existingNames = new HashSet<>();
            try (Cursor cursor = context.getContentResolver().query(children, new String[]{DocumentsContract.Document.COLUMN_DOCUMENT_ID,
                    DocumentsContract.Document.COLUMN_DISPLAY_NAME, DocumentsContract.Document.COLUMN_MIME_TYPE}, null, null, null)) {
                if (cursor == null) throw new java.io.IOException("无法读取导出文件夹");
                while (cursor.moveToNext()) {
                    existingNames.add(cursor.getString(1));
                    if (folder && name.equals(cursor.getString(1))) {
                        if (!DocumentsContract.Document.MIME_TYPE_DIR.equals(cursor.getString(2)))
                            throw new java.io.IOException("导出路径与已有文件类型冲突");
                        child = DocumentsContract.buildDocumentUriUsingTree(tree, cursor.getString(0));
                    }
                }
            }
            if (!folder) {
                int dot = name.lastIndexOf('.');
                String stem = dot > 0 ? name.substring(0, dot) : name;
                String suffix = dot > 0 ? name.substring(dot) : "";
                for (int number = 1; existingNames.contains(name); ++number) name = stem + "(" + number + ")" + suffix;
            }
            if (child == null) child = DocumentsContract.createDocument(context.getContentResolver(), parent,
                folder ? DocumentsContract.Document.MIME_TYPE_DIR : mimeForName(name), name);
            if (child == null) throw new java.io.IOException("无法创建导出文件");
            parent = child;
        }
        return parent;
    }
    private static String documentName(Context context, Uri document) throws Exception {
        try (Cursor cursor = context.getContentResolver().query(document,
                new String[]{DocumentsContract.Document.COLUMN_DISPLAY_NAME}, null, null, null)) {
            if (cursor == null || !cursor.moveToFirst() || cursor.isNull(0))
                throw new java.io.IOException("无法读取导出文件名");
            return cursor.getString(0);
        }
    }
    private static void send(String token, boolean done, boolean ok, int percent, Uri uri, String sha, long bytes, String error) {
        send(token, done, ok, percent, uri, sha, bytes, error, "");
    }
    private static void send(String token, boolean done, boolean ok, int percent, Uri uri, String sha, long bytes, String error, String displayPath) {
        try {
            JSONObject result = new JSONObject().put("kind", "exportPublish").put("token", token).put("done", done).put("ok", ok)
                .put("percent", percent).put("sha256", sha).put("bytes", bytes).put("error", error);
            if (uri != null) result.put("uri", uri.toString());
            if (!displayPath.isEmpty()) result.put("displayPath", displayPath);
            MiaCodeActivity.deliverResult(result.toString());
        } catch (Exception errorMakingResult) { android.util.Log.e("MiaCode", "Export response failed", errorMakingResult); }
    }
}
