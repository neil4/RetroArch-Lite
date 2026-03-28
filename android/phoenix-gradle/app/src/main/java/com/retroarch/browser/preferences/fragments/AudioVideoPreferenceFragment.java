package com.retroarch.browser.preferences.fragments;

import android.content.SharedPreferences;
import android.os.Bundle;
import android.preference.Preference;
import android.preference.Preference.OnPreferenceClickListener;
import android.preference.PreferenceManager;
import android.view.Display;
import android.view.WindowManager;

import com.retroarch.browser.DarkToast;
import com.retroarch.browser.preferences.fragments.util.PreferenceListFragment;
import com.retroarchlite.R;

/**
 * A {@link PreferenceListFragment} responsible for handling the video preferences.
 */
public final class AudioVideoPreferenceFragment extends PreferenceListFragment implements OnPreferenceClickListener
{
   @Override
   public void onCreate(Bundle savedInstanceState)
   {
      super.onCreate(savedInstanceState);

      // Add preferences from the resources
      addPreferencesFromResource(R.xml.audio_video_preferences);

      // Set preference click listeners
      findPreference("set_os_reported_ref_rate_pref").setOnPreferenceClickListener(this);
   }

   @Override
   public boolean onPreferenceClick(Preference preference)
   {
      final String prefKey = preference.getKey();

      // Set OS-reported refresh rate preference.
      if (prefKey.equals("set_os_reported_ref_rate_pref"))
      {
         final WindowManager wm = getActivity().getWindowManager();
         final Display display = wm.getDefaultDisplay();

         final double monitorRate = display.getRefreshRate();
         final long swapInterval = Math.max(Math.round(monitorRate / 60.0), 1);

         final SharedPreferences prefs = PreferenceManager.getDefaultSharedPreferences(getActivity());
         final SharedPreferences.Editor edit = prefs.edit();
         edit.putString("video_refresh_rate", Double.toString(monitorRate));
         edit.putString("video_swap_interval", Long.toString(swapInterval));
         edit.apply();

         DarkToast.makeText(getActivity(),
               String.format(getString(R.string.using_os_reported_refresh_rate),
                     monitorRate, swapInterval));
      }

      return true;
   }
}
