package com.avatar.fota.receiver;

import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.Intent;

import com.avatar.fota.service.OTAUpdateService;
import com.avatar.fota.utils.Util;

public class BootReceiver extends BroadcastReceiver {
    private static final String TAG = "OTAServiceBootReceiver";
    private Context mContext;

    @Override
    public void onReceive(Context context, Intent intent) {
        mContext = context;

        if (intent.getAction().equals(Intent.ACTION_BOOT_COMPLETED)) {
            Util.Logd(TAG, "Boot complete then start ota service");

            Intent otaIntent = new Intent(mContext, OTAUpdateService.class);
           // mContext.startService(otaIntent);
        }
    }
}