package com.retroarch.browser.preferences.fragments;

import com.retroarchlite.R;
import android.app.AlertDialog;
import android.app.Dialog;
import android.content.DialogInterface;
import com.retroarch.browser.dirfragment.DirectoryFragment;
import com.retroarch.browser.preferences.fragments.util.PreferenceListFragment;
import android.os.Bundle;
import android.preference.Preference;
import android.preference.Preference.OnPreferenceClickListener;
import android.widget.Toast;

/**
 * A {@link PreferenceListFragment} responsible for handling the input preferences.
 */
public final class InputPreferenceFragment extends PreferenceListFragment implements OnPreferenceClickListener
{
   @Override
   public void onCreate(Bundle savedInstanceState)
   {
      super.onCreate(savedInstanceState);
      
      // Add input preferences from the XML.
      addPreferencesFromResource(R.xml.input_preferences);

      // Set preference listeners
      findPreference("reextract_overlays_pref").setOnPreferenceClickListener(this);
      findPreference("install_overlays_pref").setOnPreferenceClickListener(this);
   }

   @Override
   public boolean onPreferenceClick(Preference preference)
   {
      final String prefKey = preference.getKey();

      if (prefKey.equals("install_overlays_pref"))
      {
         final DirectoryFragment overlayFileBrowser
                 = DirectoryFragment.newInstance("");         
         overlayFileBrowser.setPathSettingKey("overlay_zip");
         overlayFileBrowser.addAllowedExts("zip");
         overlayFileBrowser.setIsDirectoryTarget(false);
         overlayFileBrowser.show(getFragmentManager(), "overlayFileBrowser");
      }
      else if (prefKey.equals("reextract_overlays_pref"))
      {
         final DirectoryFragment overlayFileBrowser
                 = DirectoryFragment.newInstance("");
         
         AlertDialog.Builder builder = new AlertDialog.Builder(getContext());
         builder.setMessage("Confirm: Update/Restore Overlays?\nUser-installed overlays will be removed.")
            .setCancelable(true)
            .setPositiveButton("Yes",
                  new DialogInterface.OnClickListener()
                  {
                     public void onClick(DialogInterface dialog, int id)
                     {
                        boolean success = overlayFileBrowser.RestoreDirFromZip(
                                getActivity().getApplicationInfo().sourceDir,
                               "assets/overlays",
                                getActivity().getApplicationInfo().dataDir + "/overlays");
                        if (success) {
                           Toast.makeText(getContext(), "Overlays Restored.", Toast.LENGTH_SHORT).show();
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
