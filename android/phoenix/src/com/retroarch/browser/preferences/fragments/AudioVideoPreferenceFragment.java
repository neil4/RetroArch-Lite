package com.retroarch.browser.preferences.fragments;

import android.app.AlertDialog;
import android.app.Dialog;
import android.content.DialogInterface;
import com.retroarchlite.R;
import com.retroarch.browser.preferences.fragments.util.PreferenceListFragment;

import android.content.SharedPreferences;
import android.os.Bundle;
import android.preference.Preference;
import android.preference.PreferenceManager;
import android.preference.Preference.OnPreferenceClickListener;
import android.view.Display;
import android.view.WindowManager;
import android.widget.Toast;
import com.retroarch.browser.dirfragment.DirectoryFragment;

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
      findPreference("install_shaders_pref").setOnPreferenceClickListener(this);
      findPreference("reextract_shaders_pref").setOnPreferenceClickListener(this);
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
         final double rate = display.getRefreshRate();

         final SharedPreferences prefs = PreferenceManager.getDefaultSharedPreferences(getActivity());
         final SharedPreferences.Editor edit = prefs.edit();
         edit.putString("video_refresh_rate", Double.toString(rate));
         edit.apply();

         Toast.makeText(getActivity(), String.format(getString(R.string.using_os_reported_refresh_rate), rate), Toast.LENGTH_LONG).show();
      }
      else if (prefKey.equals("install_shaders_pref"))
      {
         final DirectoryFragment shaderFileBrowser
                 = DirectoryFragment.newInstance("");         
         shaderFileBrowser.setPathSettingKey("shader_zip");
         shaderFileBrowser.addAllowedExts("zip");
         shaderFileBrowser.setIsDirectoryTarget(false);
         shaderFileBrowser.show(getFragmentManager(), "shaderFileBrowser");
      }
      else if (prefKey.equals("reextract_shaders_pref"))
      {
         final DirectoryFragment shaderFileBrowser
                 = DirectoryFragment.newInstance("");
         
         AlertDialog.Builder builder = new AlertDialog.Builder(getContext());
         builder.setMessage("Confirm: Update/Restore Shaders and Presets?\nUser-installed shaders will be removed.")
            .setCancelable(true)
            .setPositiveButton("Yes",
                  new DialogInterface.OnClickListener()
                  {
                     public void onClick(DialogInterface dialog, int id)
                     {
                        boolean success = shaderFileBrowser.RestoreDirFromZip(
                                getActivity().getApplicationInfo().sourceDir,
                                "assets/shaders_glsl",
                                getActivity().getApplicationInfo().dataDir + "/shaders_glsl");
                        if (success) {
                           Toast.makeText(getContext(), "Shaders Restored.", Toast.LENGTH_SHORT).show();
                        }
                     }
                  })
            .setNegativeButton("No", new DialogInterface.OnClickListener()
            {
               public void onClick(DialogInterface dialog, int id) {}
            });
         Dialog dialog = builder.create();
         dialog.show();
      }

      return true;
   }
}
