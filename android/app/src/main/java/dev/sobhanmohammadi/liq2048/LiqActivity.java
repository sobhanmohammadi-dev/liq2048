package dev.sobhanmohammadi.liq2048;

import android.content.ClipData;
import android.content.ClipboardManager;
import android.content.ContentResolver;
import android.content.ContentValues;
import android.content.Context;
import android.net.Uri;
import android.os.Build;
import android.os.Environment;
import android.provider.MediaStore;

import org.libsdl.app.SDLActivity;

import java.io.File;
import java.io.FileInputStream;
import java.io.InputStream;
import java.io.OutputStream;
import java.util.concurrent.CountDownLatch;
import java.util.concurrent.TimeUnit;

/** The whole game runs in native code; SDL's activity hosts the surface, input and audio. */
public class LiqActivity extends SDLActivity {
    @Override
    protected String[] getLibraries() {
        return new String[] {"SDL2", "main"};
    }

    /**
     * Called from native code. Saves the PNG into the gallery (Pictures/Liq2048, no permission needed on Android 10+)
     * and puts that picture on the clipboard. Returns bit 1 = saved, bit 2 = copied.
     */
    public int exportImage(final String pngPath, final String name) {
        int result = 0;
        Uri uri = null;
        try {
            final ContentResolver cr = getContentResolver();
            if (Build.VERSION.SDK_INT >= 29) {
                ContentValues v = new ContentValues();
                v.put(MediaStore.Images.Media.DISPLAY_NAME, name);
                v.put(MediaStore.Images.Media.MIME_TYPE, "image/png");
                v.put(MediaStore.Images.Media.RELATIVE_PATH, Environment.DIRECTORY_PICTURES + "/Liq2048");
                // Same name as an earlier shot? The system adds a (1) suffix, so nothing is overwritten.
                uri = cr.insert(MediaStore.Images.Media.EXTERNAL_CONTENT_URI, v);
                if (uri != null) {
                    try (InputStream in = new FileInputStream(pngPath); OutputStream out = cr.openOutputStream(uri)) {
                        byte[] buf = new byte[16384];
                        int n;
                        while ((n = in.read(buf)) > 0) out.write(buf, 0, n);
                    }
                    result |= 1;
                }
            } else {
                // Android 7-9: without a storage permission the picture goes to the app's own Pictures folder.
                File dir = getExternalFilesDir(Environment.DIRECTORY_PICTURES);
                if (dir != null) {
                    dir.mkdirs();
                    try (InputStream in = new FileInputStream(pngPath);
                         OutputStream out = new java.io.FileOutputStream(new File(dir, name))) {
                        byte[] buf = new byte[16384];
                        int n;
                        while ((n = in.read(buf)) > 0) out.write(buf, 0, n);
                    }
                    result |= 1;
                }
            }
        } catch (Exception e) {
            uri = null;
        }
        if (uri != null) {
            final Uri u = uri;
            final boolean[] ok = {false};
            final CountDownLatch done = new CountDownLatch(1);
            runOnUiThread(new Runnable() {
                @Override
                public void run() {
                    try {
                        ClipboardManager cm = (ClipboardManager) getSystemService(Context.CLIPBOARD_SERVICE);
                        cm.setPrimaryClip(ClipData.newUri(getContentResolver(), "Liq2048", u));
                        ok[0] = true;
                    } catch (Exception e) {
                        ok[0] = false;
                    }
                    done.countDown();
                }
            });
            try {
                done.await(2, TimeUnit.SECONDS);
            } catch (InterruptedException e) {
                Thread.currentThread().interrupt();
            }
            if (ok[0]) result |= 2;
        }
        return result;
    }
}
