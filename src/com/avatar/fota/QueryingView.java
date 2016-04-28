package com.avatar.fota;

import android.content.Context;
import android.util.AttributeSet;
import android.view.View;
import android.view.animation.Animation;
import android.view.animation.AnimationUtils;
import android.widget.ImageView;
import android.widget.LinearLayout;
import android.widget.RelativeLayout;
import android.widget.TextView;


import com.avatar.fota.utils.Util;


public class QueryingView extends RelativeLayout {
    private static final String TAG = "QueryingView";

    private ImageView mAnimView;
    private Animation mAnimation;
    private TextView mDetailView;

    public QueryingView(Context context) {
        super(context);
    }

    public QueryingView(Context context, AttributeSet attrs) {
        super(context, attrs);
    }

    @Override
    protected void onAttachedToWindow() {
        super.onAttachedToWindow();
        mAnimView.startAnimation(mAnimation);
    }

    @Override
    protected void onDetachedFromWindow() {
        super.onDetachedFromWindow();
        mAnimView.clearAnimation();
    }

    @Override
    protected void onFinishInflate() {
        mAnimation = AnimationUtils.loadAnimation(getContext(), R.anim.query_progress);
        mAnimView = (ImageView)findViewById(R.id.loadingView);

        mDetailView = (TextView)findViewById(R.id.detail_text);
    }

    public void setErrorInfo(String s) {
        mAnimView.clearAnimation();
        mAnimView.setVisibility(View.GONE);

        mDetailView.setText(s);
    }

}
