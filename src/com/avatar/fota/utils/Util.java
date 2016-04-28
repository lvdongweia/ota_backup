package com.avatar.fota.utils;


import android.os.Environment;
import android.os.StatFs;
import android.util.Log;

import java.io.File;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.io.OutputStream;
import java.text.DecimalFormat;
import java.text.ParseException;
import java.text.SimpleDateFormat;
import java.util.ArrayList;
import java.util.Date;
import java.util.Enumeration;
import java.util.List;
import java.util.zip.ZipEntry;
import java.util.zip.ZipException;
import java.util.zip.ZipFile;


public class Util {
    private static final String TAG = "OTAUpdate";
    private static final boolean DEBUG = true;

    static final DecimalFormat DOUBLE_DECIMAL_FORMAT = new DecimalFormat("0.##");
    private static final int MB = 1024 * 1024;
    private static final int KB = 1024;

    public static void Logv(String tag, String msg) {
        if (DEBUG) {
            Log.v(TAG, "[" + tag + "]:" + msg);
        }
    }

    public static void Logd(String tag, String msg) {
        if (DEBUG) {
            Log.d(TAG, "[" + tag + "]:" + msg);
        }
    }

    public static void Logi(String tag, String msg) {
        if (DEBUG) {
            Log.i(TAG, "[" + tag + "]:" + msg);
        }
    }

    public static void Logw(String tag, String msg) {
        if (DEBUG) {
            Log.w(TAG, "[" + tag + "]:" + msg);
        }
    }

    public static void Loge(String tag, String msg) {
        if (DEBUG) {
            Log.e(TAG, "[" + tag + "]:" + msg);
        }
    }

    public static String getTimeNow() {
        SimpleDateFormat sdf = new SimpleDateFormat("yyyy-MM-dd HH:mm:ss");
        Date nowTime = new Date();
        String dateStr = sdf.format(nowTime);
        return dateStr;
    }

    public static long getTimeDiff(String now, String old) {
        SimpleDateFormat sdf = new SimpleDateFormat("yyyy-MM-dd HH:mm:ss");
        long diffTime = -1;
        try {
            Date nowDate = sdf.parse(now);
            Date oldDate = sdf.parse(old);

            diffTime = Math.abs(nowDate.getTime() - oldDate.getTime());
        } catch (ParseException e) {
            e.printStackTrace();
        }

        return diffTime;
    }

    public static CharSequence getFileSize(long size) {
        if (size <= 0)
            return "0M";

        if (size >= MB) {
            return new StringBuffer(16).append(
                DOUBLE_DECIMAL_FORMAT.format((double)size / MB)).append(" MB");
        }
        else if (size >= KB) {
            return new StringBuilder(16).append(
                DOUBLE_DECIMAL_FORMAT.format((double)size / KB)).append(" KB");
        }
        else {
            return size + " B";
        }
    }

    public static File[] unzipFile(String zipPath, String folderPath)
        throws ZipException, IOException {
        long start = System.currentTimeMillis();
        List<File> files = new ArrayList<File>();

        File zipFile = new File(zipPath);
        File desDir = new File(folderPath);
        if (!desDir.exists()) {
            desDir.mkdirs();
        }

        ZipFile zf = new ZipFile(zipFile);
        for (Enumeration<?> entries = zf.entries(); entries.hasMoreElements();) {
            ZipEntry entry = (ZipEntry)entries.nextElement();
            String name = entry.getName();
            Logd(TAG, "Unzip file: " + name);

            InputStream in = zf.getInputStream(entry);
            File dstFile = new File(folderPath + File.separator + name);
            if (!dstFile.exists()) {
                File fileParentDir = dstFile.getParentFile();
                if (!fileParentDir.exists()) {
                    fileParentDir.mkdirs();
                }
                dstFile.createNewFile();
            }

            OutputStream out = new FileOutputStream(dstFile);
            byte[] buffer = new byte[4096];
            int readLen = 0;
            while ((readLen = in.read(buffer)) > 0) {
                out.write(buffer, 0, readLen);
            }
            in.close();
            out.close();

            // save file name
            files.add(dstFile);
        }

        long end = System.currentTimeMillis();
        if (DEBUG) {
            Logd(TAG, "Unzip use time:" + (end - start) / 1000 + "s");
        }

        File[] tol = new File[files.size()];
        return files.toArray(tol);
    }

    // get available internal sd size
    public static long getInternalMemorySize() {
        StatFs stat = new StatFs(ConstValue.DOWNLOAD_PATH);
        long blockSize = stat.getBlockSizeLong();
        long availableBlocks = stat.getAvailableBlocksLong();

        return blockSize * availableBlocks;
    }
}
