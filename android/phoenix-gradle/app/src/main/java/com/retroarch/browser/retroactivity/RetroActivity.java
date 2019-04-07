package com.retroarch.browser.retroactivity;

import android.os.Build;
import android.view.View;
import com.retroarch.browser.preferences.util.UserPreferences;

public class RetroActivity extends RetroActivityLocation
{
	@Override
	public void onLowMemory()
	{
	}

	@Override
	public void onTrimMemory(int level)
	{
	}
   
   @Override
	public void onResume() {
		super.onResume();

      if ((Build.VERSION.SDK_INT >= Build.VERSION_CODES.HONEYCOMB))
      {
         View thisView = getWindow().getDecorView();
         thisView.setSystemUiVisibility(
                 View.SYSTEM_UI_FLAG_LAYOUT_FULLSCREEN
               | View.SYSTEM_UI_FLAG_HIDE_NAVIGATION
               | View.SYSTEM_UI_FLAG_IMMERSIVE_STICKY );
      }
	}

	// Exiting cleanly from NDK seems to be nearly impossible.
	// Have to use exit(0) to avoid weird things happening, even with runOnUiThread() approaches.
	// Use a separate JNI function to explicitly trigger the readback.
	public void onRetroArchExit()
	{
		UserPreferences.readbackConfigFile(this);
	}
}
