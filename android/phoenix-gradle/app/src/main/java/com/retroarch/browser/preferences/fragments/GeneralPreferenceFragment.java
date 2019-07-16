package com.retroarch.browser.preferences.fragments;

import android.app.AlertDialog;
import android.app.Dialog;
import android.content.ComponentName;
import android.content.DialogInterface;
import android.content.Intent;

import com.retroarch.browser.NativeInterface;
import com.retroarchlite.R;
import com.retroarch.browser.preferences.fragments.util.PreferenceListFragment;

import android.os.Bundle;
import android.preference.Preference;
import android.preference.Preference.OnPreferenceClickListener;
import android.widget.Toast;
import com.retroarch.browser.dirfragment.DirectoryFragment;

/**
 * A {@link PreferenceListFragment} that handles the general settings.
 */
public final class GeneralPreferenceFragment extends PreferenceListFragment implements OnPreferenceClickListener
{
   @Override
   public void onCreate(Bundle savedInstanceState)
   {
      super.onCreate(savedInstanceState);

      // Add general preferences from the XML.
      if (haveSharedCoreManager())
         addPreferencesFromResource(R.xml.general_preferences_32_64);
      else
         addPreferencesFromResource(R.xml.general_preferences);
      
      // Set preference listeners
      findPreference("install_themes_pref").setOnPreferenceClickListener(this);
      findPreference("restore_assets_pref").setOnPreferenceClickListener(this);
   }
   
   protected boolean haveSharedCoreManager()
   {
      final String sharedId = getString(R.string.shared_app_id);
      Intent intent = new Intent();
      intent.setComponent(new ComponentName(sharedId,
                                            "com.retroarch.browser.coremanager.CoreManagerActivity"));
      return !getContext().getPackageManager()
                          .queryIntentActivities(intent, 0)
                          .isEmpty();
   }
   
   @Override
   public boolean onPreferenceClick(Preference preference)
   {
      final String prefKey = preference.getKey();

      if (prefKey.equals("install_themes_pref"))
      {
         final DirectoryFragment themeFileBrowser
                 = DirectoryFragment.newInstance("");
         themeFileBrowser.setPathSettingKey("themes_zip");
         themeFileBrowser.addAllowedExts("zip");
         themeFileBrowser.setIsDirectoryTarget(false);
         themeFileBrowser.show(getFragmentManager(), "themeFileBrowser");
      }
      else if (prefKey.equals("restore_assets_pref"))
      {
         final DirectoryFragment assetFileBrowser
               = DirectoryFragment.newInstance("");

         AlertDialog.Builder builder = new AlertDialog.Builder(getContext());
         builder.setMessage("Confirm: Restore all assets?\nUser-installed assets will be removed.")
               .setCancelable(true)
               .setPositiveButton("Yes",
                     new DialogInterface.OnClickListener()
                     {
                        public void onClick(DialogInterface dialog, int id)
                        {
                           boolean success = assetFileBrowser.RestoreDirFromZip(
                                 getActivity().getApplicationInfo().sourceDir,
                                 "assets/themes_rgui",
                                 getActivity().getApplicationInfo().dataDir + "/themes_rgui");
                           success &= assetFileBrowser.RestoreDirFromZip(
                                 getActivity().getApplicationInfo().sourceDir,
                                 "assets/audio_filters",
                                 getActivity().getApplicationInfo().dataDir + "/audio_filters");
                           success &= assetFileBrowser.RestoreDirFromZip(
                                 getActivity().getApplicationInfo().sourceDir,
                                 "assets/video_filters",
                                 getActivity().getApplicationInfo().dataDir + "/video_filters");
                           success &= assetFileBrowser.RestoreDirFromZip(
                                 getActivity().getApplicationInfo().sourceDir,
                                 "assets/shaders_glsl",
                                 getActivity().getApplicationInfo().dataDir + "/shaders_glsl");
                           success &= assetFileBrowser.RestoreDirFromZip(
                                 getActivity().getApplicationInfo().sourceDir,
                                 "assets/overlays",
                                 getActivity().getApplicationInfo().dataDir + "/overlays");
                           success &= NativeInterface.extractArchiveTo(
                                 getActivity().getApplicationInfo().sourceDir,
                                 "assets/info",
                                 getActivity().getApplicationInfo().dataDir + "/info");
                           if (success) {
                              Toast.makeText(getContext(), "Assets restored.", Toast.LENGTH_SHORT).show();
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
