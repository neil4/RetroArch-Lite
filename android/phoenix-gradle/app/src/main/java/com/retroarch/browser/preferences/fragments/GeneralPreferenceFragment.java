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
import android.widget.Toast;

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
      if (haveSharedCoreManager())
         addPreferencesFromResource(R.xml.general_preferences_32_64);
      else
         addPreferencesFromResource(R.xml.general_preferences);
      
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
                        String folders[] = {"/overlays", "/info", "/shaders_glsl", "/themes_rgui", "/video_filters", "/audio_filters"};

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
                              Toast.makeText(ctx, "Assets restored.", Toast.LENGTH_SHORT).show();
                           else
                              Toast.makeText(ctx, "Failed to restore assets.", Toast.LENGTH_SHORT).show();
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
      String folders[] = {"overlays", "info", "shaders_glsl", "themes_rgui"};
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
                           Toast.makeText(ctx, "Assets installed.", Toast.LENGTH_SHORT).show();
                        else
                           Toast.makeText(ctx, "Failed to extract assets.", Toast.LENGTH_SHORT).show();
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
