package com.retroarch.browser.preferences.fragments;

import android.app.AlertDialog;
import android.app.Dialog;
import android.content.ComponentName;
import android.content.Context;
import android.content.DialogInterface;
import android.content.Intent;
import android.os.Bundle;
import android.preference.Preference;
import android.preference.Preference.OnPreferenceClickListener;
import android.preference.PreferenceCategory;

import com.retroarch.browser.DarkToast;
import com.retroarch.browser.NativeInterface;
import com.retroarch.browser.dirfragment.DirectoryFragment;
import com.retroarch.browser.preferences.fragments.util.PreferenceListFragment;
import com.retroarchlite.R;

/**
 * A {@link PreferenceListFragment} that handles the general settings.
 */
public final class GeneralPreferenceFragment extends PreferenceListFragment implements OnPreferenceClickListener, DirectoryFragment.OnDirectoryFragmentClosedListener
{
   private Context ctx = null;

   @Override
   public void onCreate(Bundle savedInstanceState)
   {
      super.onCreate(savedInstanceState);

      ctx = getContext();

      // Add general preferences from the XML.
      addPreferencesFromResource(R.xml.general_preferences);

      // Hide preferences as needed
      PreferenceCategory catCoreMgr = (PreferenceCategory) findPreference("cat_core_management");
      PreferenceCategory catUi = (PreferenceCategory) findPreference("cat_ui");
      if (haveSharedCoreManager())
         catCoreMgr.removePreference(findPreference("manage_cores"));
      else
      {
         catCoreMgr.removePreference(findPreference("manage_cores32"));
         catCoreMgr.removePreference(findPreference("manage_cores64"));
         catUi.removePreference(findPreference("append_abi_to_corenames"));
      }
      
      // Set preference listeners
      findPreference("install_assets_pref").setOnPreferenceClickListener(this);
      findPreference("restore_assets_pref").setOnPreferenceClickListener(this);
   }
   
   protected boolean haveSharedCoreManager()
   {
      final String sharedId = getString(R.string.shared_app_id);
      Intent intent = new Intent();
      intent.setComponent(new ComponentName(sharedId,
                                            "com.retroarch.browser.coremanager.CoreManagerActivity"));
      return !ctx.getPackageManager().queryIntentActivities(intent, 0).isEmpty();
   }
   
   @Override
   public boolean onPreferenceClick(Preference preference)
   {
      final String prefKey = preference.getKey();

      if (prefKey.equals("install_assets_pref"))
      {
         final DirectoryFragment assetsFileBrowser
                 = DirectoryFragment.newInstance(R.string.install_assets);
         assetsFileBrowser.addAllowedExts("zip");
         assetsFileBrowser.setIsDirectoryTarget(false);
         assetsFileBrowser.setOnDirectoryFragmentClosedListener(this);
         assetsFileBrowser.show(getFragmentManager(), "assetsFileBrowser");
      }
      else if (prefKey.equals("restore_assets_pref"))
      {
         AlertDialog.Builder builder = new AlertDialog.Builder(ctx);
         builder.setMessage("Confirm: Restore all assets?\nUser-installed assets will be removed.")
               .setCancelable(true)
               .setPositiveButton("Yes",
                     new DialogInterface.OnClickListener()
                     {
                        String folders[] = {"/overlays", "/info", "/shaders_glsl", "/themes_rgui", "/video_filters", "/audio_filters", "/autoconfig"};

                        public void onClick(DialogInterface dialog, int id)
                        {
                           boolean success = true;

                           for (String folder : folders)
                           {
                              success &= NativeInterface.RestoreDirFromZip(
                                    getActivity().getApplicationInfo().sourceDir,
                                    "assets" + folder,
                                    getActivity().getApplicationInfo().dataDir + folder);
                           }

                           if (success)
                              DarkToast.makeText(ctx, "Assets restored.");
                           else
                              DarkToast.makeText(ctx, "Failed to restore assets.");
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

   @Override
   public void onDirectoryFragmentClosed(String path)
   {
      String folders[] = {"overlays", "info", "shaders_glsl", "themes_rgui", "autoconfig"};
      String dataDir = ctx.getApplicationInfo().dataDir;

      AlertDialog.Builder builder = new AlertDialog.Builder(ctx);
      builder.setMessage("Confirm: Extract assets from " + path.substring(path.lastIndexOf('/')+1) + "?")
            .setCancelable(true)
            .setPositiveButton("Yes",
                  new DialogInterface.OnClickListener()
                  {
                     public void onClick(DialogInterface dialog, int id)
                     {
                        boolean success = false;

                        for (String folder : folders)
                           success |= NativeInterface.extractArchiveTo(path, folder, dataDir + '/' + folder);

                        if (success)
                           DarkToast.makeText(ctx, "Assets installed.");
                        else
                           DarkToast.makeText(getActivity(), "Failed to extract assets.");
                     }
                  })
            .setNegativeButton("No", new DialogInterface.OnClickListener()
            {
               public void onClick(DialogInterface dialog, int id) {}
            });
      Dialog dialog = builder.create();
      dialog.show();
   }
}
