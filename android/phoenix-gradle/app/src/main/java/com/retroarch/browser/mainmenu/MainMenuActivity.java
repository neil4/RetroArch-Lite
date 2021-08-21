package com.retroarch.browser.mainmenu;

import android.Manifest;
import android.app.Activity;
import android.app.Dialog;
import android.content.ComponentName;
import android.content.Context;
import android.content.Intent;
import android.content.SharedPreferences;
import android.content.pm.PackageManager;
import android.media.AudioManager;
import android.os.Bundle;
import android.os.Handler;
import android.preference.PreferenceManager;
import android.provider.Settings;
import androidx.core.app.ActivityCompat;
import androidx.fragment.app.FragmentActivity;
import androidx.core.content.ContextCompat;
import android.util.Log;
import android.view.KeyEvent;
import android.view.Menu;
import android.view.MenuItem;
import android.view.View;
import android.widget.AdapterView;
import android.widget.ListView;
import android.widget.Toast;

import com.retroarch.browser.IconAdapter;
import com.retroarch.browser.ModuleWrapper;
import com.retroarch.browser.NativeInterface;
import com.retroarch.browser.coremanager.fragments.InstalledCoresFragment;
import com.retroarch.browser.dirfragment.DirectoryFragment;
import com.retroarch.browser.preferences.PreferenceActivity;
import com.retroarch.browser.preferences.util.UserPreferences;
import com.retroarch.browser.retroactivity.RetroActivity;
import com.retroarchlite.R;

import java.io.DataInputStream;
import java.io.DataOutputStream;
import java.io.File;
import java.io.FileInputStream;
import java.io.FileOutputStream;
import java.io.IOException;
import java.util.ArrayList;
import java.util.Collections;
import java.util.List;

/**
 * {@link FragmentActivity} subclass that provides all of the
 * functionality of the main menu screen.
 */
public final class MainMenuActivity extends FragmentActivity implements DirectoryFragment.OnDirectoryFragmentClosedListener
{
   private static final int REQUEST_APP_PERMISSIONS = 88;
   private IconAdapter<ModuleWrapper> adapter;
   private String libretroPath = "";
   private String libretroName = "";
   public static Intent retro = null;

   @Override
   public void onCreate(Bundle savedInstanceState)
   {
      super.onCreate(savedInstanceState);

      if (retro != null)
      {
         startActivity(retro);
         finish();
      }

      extractAssets();

      // Load the main menu layout
      setContentView(R.layout.line_list);
      createList();

      // Bind audio stream to hardware controls.
      setVolumeControlStream(AudioManager.STREAM_MUSIC);

      if (android.os.Build.VERSION.SDK_INT >= 23)
         getPermissions(this, this);
   }

   public static void getPermissions(Context ctx, Activity act)
   {
      final String[] permissions = {Manifest.permission.WRITE_EXTERNAL_STORAGE};
      boolean anyDenied = false;

      for (int i = 0; i < permissions.length; i++)
      {
         int code = ContextCompat.checkSelfPermission(ctx, permissions[i]);
         if (code != PackageManager.PERMISSION_GRANTED)
            anyDenied = true;
      }

      if (anyDenied)
         ActivityCompat.requestPermissions(act, permissions, REQUEST_APP_PERMISSIONS);
   }

   @Override
   public void onRequestPermissionsResult(int requestCode, String[] permissions, int[] grantResults)
   {
      switch (requestCode)
      {
         case REQUEST_APP_PERMISSIONS:
            for (int i = 0; i < permissions.length; i++)
            {
               if(grantResults[i] != PackageManager.PERMISSION_GRANTED)
                  Log.e("RetroArch Lite",permissions[i] + " denied");
            }
            break;
         default:
            super.onRequestPermissionsResult(requestCode, permissions, grantResults);
            break;
      }
   }

   @Override
   protected void onSaveInstanceState(Bundle data)
   {
      super.onSaveInstanceState(data);

      data.putCharSequence("title", getTitle());
   }

   @Override
   protected void onRestoreInstanceState(Bundle savedInstanceState)
   {
      super.onRestoreInstanceState(savedInstanceState);

      setTitle(savedInstanceState.getCharSequence("title"));
   }

   @Override
   public boolean onCreateOptionsMenu(Menu aMenu)
   {
      super.onCreateOptionsMenu(aMenu);
      getMenuInflater().inflate(R.menu.options_menu, aMenu);
      return true;
   }
   
   @Override
   public boolean onOptionsItemSelected(MenuItem aItem)
   {
      if (aItem.getItemId() == R.id.settings)
      {
         Intent rset = new Intent(this, PreferenceActivity.class);
         startActivity(rset);
         return true;
      }
      return super.onOptionsItemSelected(aItem);
   }

   @Override
   public boolean onKeyDown (int keyCode,
                             KeyEvent event)
   {
      ListView coreList = findViewById(R.id.list);

      if (coreList.hasFocus()
            && event.getAction() == android.view.KeyEvent.ACTION_DOWN
            && keyCode == android.view.KeyEvent.KEYCODE_BUTTON_SELECT)
      {
         startActivity(new Intent(this, PreferenceActivity.class));
      }

      return super.onKeyDown(keyCode, event);
   }
   
   public void createList()
   {
      SharedPreferences prefs = UserPreferences.getPreferences(getApplicationContext());
      boolean showAbi = prefs.getBoolean("append_abi_to_corenames", false);
      boolean sortBySys = prefs.getBoolean("sort_cores_by_system", true);

      // Inflate the ListView we're using.
      ListView coreList = (ListView) findViewById(R.id.list);

      // Populate local core list
      //
      final List<ModuleWrapper> cores = new ArrayList<ModuleWrapper>();
      File coreDir = new File(getApplicationInfo().dataDir, "cores");
      if (!coreDir.exists())
         coreDir.mkdir();
      final File[] libs = coreDir.listFiles();
      
      for (final File lib : libs)
         cores.add(new ModuleWrapper(this, lib, showAbi, sortBySys));
      
      // Populate shared core list
      //
      final String sharedId = getString(R.string.shared_app_id);
      String sharedDataDir;
      if (sharedId.endsWith("64"))
         sharedDataDir = getApplicationInfo().dataDir.replaceFirst("retroarchlite", "retroarchlite64");
      else
         sharedDataDir = getApplicationInfo().dataDir.replaceFirst("retroarchlite64", "retroarchlite");
      
      File sharedCoreDir = new File(sharedDataDir, "cores");
      if (sharedCoreDir.exists())
      {
         final File[] sharedLibs = sharedCoreDir.listFiles();
         for (final File lib : sharedLibs)
            cores.add(new ModuleWrapper(this, lib, showAbi, sortBySys));
      }

      // Sort the list of cores alphabetically
      Collections.sort(cores);

      // Initialize the IconAdapter with the list of cores.
      adapter = new IconAdapter<ModuleWrapper>(this, R.layout.line_list_item, cores);
      coreList.setAdapter(adapter);
      coreList.setOnItemClickListener(onClickListener);
   }
  
   public void itemClick(AdapterView<?> parent, View view, int position, long id)
   {
      final ModuleWrapper item = adapter.getItem(position);
      libretroPath = item.getUnderlyingFile().getAbsolutePath();
      libretroName = InstalledCoresFragment.sanitizedLibretroName(libretroPath);

      // Show Content Directory
      //
      final SharedPreferences prefs = PreferenceManager.getDefaultSharedPreferences(this);

      if (!new File(libretroPath).isDirectory())
      {
         DirectoryFragment contentBrowser = DirectoryFragment.newInstance(item.getCoreTitle(),
               item.getSupportedExtensions());
         
         // Assume no content if no file extensions
         if (contentBrowser == null)
         {
            onDirectoryFragmentClosed("");
            return;
         }

         contentBrowser.setShowMameTitles(prefs.getBoolean("mame_titles", false));
         contentBrowser.setOnDirectoryFragmentClosedListener(this);

         String startPath = prefs.getString(libretroName + "_directory", "");
         if (startPath.isEmpty() || !new File(startPath).exists())
            startPath = prefs.getString("rgui_browser_directory", "");
         if (!startPath.isEmpty() && new File(startPath).exists())
            contentBrowser.setStartDirectory(startPath);

         contentBrowser.show(getSupportFragmentManager(), "contentBrowser");
      }
   }
   
   private final AdapterView.OnItemClickListener onClickListener = new AdapterView.OnItemClickListener()
   {
      @Override
      public void onItemClick(AdapterView<?> listView, View view, int position, long id)
      {
         itemClick(listView, view, position, id);
      }
   };
   
   private void extractAssets()
   {
      if (areAssetsExtracted())
         return;

      final Dialog dialog = new Dialog(this);
      final Handler handler = new Handler();
      dialog.setContentView(R.layout.assets);
      dialog.setCancelable(false);
      dialog.setTitle(R.string.asset_extraction);

      Thread assetsThread = new Thread(new Runnable()
      {
         public void run()
         {
            extractAssetsThread();
            handler.post(dialog::dismiss);
         }
      });
      assetsThread.start();

      dialog.show();
      try{ assetsThread.join(); }
      catch (InterruptedException ignored) {}
   }

   // Extract assets from native code. Doing it from Java side is apparently unbearably slow ...
   private void extractAssetsThread()
   {
      try
      {
         final String dataDir = getApplicationInfo().dataDir;
         final String apk = getApplicationInfo().sourceDir;

         boolean success = NativeInterface.extractArchiveTo(apk, "assets", dataDir);
         if (!success)
            throw new IOException("Failed to extract assets ...");

         File cacheVersion = new File(dataDir, ".cacheversion");
         DataOutputStream outputCacheVersion = new DataOutputStream(new FileOutputStream(cacheVersion, false));
         outputCacheVersion.writeInt(getVersionCode());
         outputCacheVersion.close();
      }
      catch (IOException ignored)
      {}
   }

   private boolean areAssetsExtracted()
   {
      int version = getVersionCode();

      try
      {
         String dataDir = getApplicationInfo().dataDir;
         File cacheVersion = new File(dataDir, ".cacheversion");
         if (cacheVersion.isFile() && cacheVersion.canRead() && cacheVersion.canWrite())
         {
            DataInputStream cacheStream = new DataInputStream(new FileInputStream(cacheVersion));
            int currentCacheVersion = 0;
            try
            {
               currentCacheVersion = cacheStream.readInt();
               cacheStream.close();
            }
            catch (IOException ignored)
            {
            }

            if (currentCacheVersion == version)
            {
               Log.i("ASSETS", "Assets already extracted, skipping...");
               return true;
            }
         }
      }
      catch (java.io.FileNotFoundException e)
      {
         return false;
      }

      return false;
   }

   private int getVersionCode()
   {
      int version = 0;
      try
      {
         version = getPackageManager().getPackageInfo(getPackageName(), 0).versionCode;
      }
      catch (PackageManager.NameNotFoundException ignored)
      {
      }

      return version;
   }

   @Override
   public void onDirectoryFragmentClosed(String path)
   {
      if (!path.isEmpty())
      {
         SharedPreferences.Editor edit = UserPreferences.getPreferences(this).edit();
         edit.putString( libretroName + "_directory",
                         path.substring(0, path.lastIndexOf( "/" )) ).apply();

         Toast.makeText(this, String.format(getString(R.string.loading_data), path),
               Toast.LENGTH_SHORT).show();
      }

      String currentIme = Settings.Secure.getString(getContentResolver(), Settings.Secure.DEFAULT_INPUT_METHOD);
      
      boolean usingSharedActivity = false;
      final String sharedId = getString(R.string.shared_app_id);
      if (libretroPath.contains(sharedId+'/'))
      {
         usingSharedActivity = true;
         retro = new Intent();
         retro.setComponent(new ComponentName(sharedId,
               "com.retroarch.browser.retroactivity.RetroActivity"));
      }
      else
         retro = new Intent(this, RetroActivity.class);

      if (!path.isEmpty())
         retro.putExtra("ROM", path);
      retro.putExtra("LIBRETRO", libretroPath);
      retro.putExtra("CONFIGFILE", UserPreferences.getDefaultConfigPath(this));
      retro.putExtra("IME", currentIme);
      retro.putExtra("DATADIR", getApplicationInfo().dataDir);
      retro.putExtra("EXTDIR", UserPreferences.defaultBaseDir);

      startActivity(retro);

      if (usingSharedActivity)
         retro = null;

      finish();
   }
   
   @Override
   public void onResume()
   {
      super.onResume();

      if (retro != null)
      {
         startActivity(retro);
         finish();
      }
      else
         createList();  // assume a core was added/updated/removed
   }

}