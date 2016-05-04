package com.avatar.fota;

import android.app.Dialog;
import android.content.Context;
import android.view.WindowManager;


public class InstallDialog extends Dialog {

    public InstallDialog(Context context) {
        super(context);
    }

    public InstallDialog(Context context, int theme) {
        super(context, theme);
        setContentView(R.layout.install_view);
        getWindow().setType(WindowManager.LayoutParams.TYPE_KEYGUARD_DIALOG);
    }

}
