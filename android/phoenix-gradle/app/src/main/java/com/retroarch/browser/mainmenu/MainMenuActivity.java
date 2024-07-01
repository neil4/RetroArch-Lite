package com.retroarch.browser.mainmenu;

import android.Manifest;
import android.app.Activity;
import android.app.Dialog;
import android.content.ActivityNotFoundException;
import android.content.ComponentName;
import android.content.Context;
import android.content.Intent;
import android.content.SharedPreferences;
import android.content.pm.PackageManager;
import android.media.AudioManager;
import android.net.Uri;
import android.os.Environment;
import android.os.Build;
import android.os.Bundle;
import android.os.Handler;
import android.preference.PreferenceManager;
import android.provider.Settings;
import androidx.fragment.app.FragmentActivity;
import android.util.Log;
import android.view.ContextMenu;
import android.view.KeyEvent;
import android.view.Menu;
import android.view.MenuInflater;
import android.view.MenuItem;
import android.view.View;
import android.widget.AdapterView;
import android.widget.ListView;

import com.retroarch.browser.DarkToast;
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

      checkRuntimePermissions(this);

      // Clear extraction directory in case app closed prematurely
      clearTemporaryStorage(getApplicationContext());
   }

   public static void checkRuntimePermissions(Activity act)
   {
      // Android 6.0+ needs runtime permission checks
      if (Build.VERSION.SDK_INT < 23)
         return;
      else if (Build.VERSION.SDK_INT < 30)
      {
         if (act.checkSelfPermission(Manifest.permission.WRITE_EXTERNAL_STORAGE)
               != PackageManager.PERMISSION_GRANTED)
         {
            Log.i("RuntimePermissions", "Requesting WRITE_EXTERNAL_STORAGE");

            act.requestPermissions(new String[]{Manifest.permission.WRITE_EXTERNAL_STORAGE},
                  REQUEST_APP_PERMISSIONS);
         }
      }
      else if (!Environment.isExternalStorageManager())
      {
         Log.i("RuntimePermissions", "Requesting MANAGE_EXTERNAL_STORAGE");

         Intent intent = new Intent(Settings.ACTION_MANAGE_APP_ALL_FILES_ACCESS_PERMISSION);
         intent.setData(Uri.fromParts("package", act.getPackageName(), null));

         try
         {
            act.startActivity(intent);
         }
         catch (ActivityNotFoundException e)
         {
            // Redirect to app info page instead, so that the user can manually grant the permission
            String text = "Navigate to Permissions -> Files and media -> Allow all the time";
            DarkToast.makeText(act, text);

            intent = new Intent(Settings.ACTION_APPLICATION_DETAILS_SETTINGS);
            intent.setData(Uri.fromParts("package", act.getPackageName(), null));
            act.startActivity(intent);
         }
      }
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
                  Log.e("RuntimePermissions",permissions[i] + " denied");
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
            && event.getAction() == KeyEvent.ACTION_DOWN
            && (keyCode == KeyEvent.KEYCODE_BUTTON_SELECT || keyCode == KeyEvent.KEYCODE_MENU))
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
      registerForContextMenu(coreList);

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

         contentBrowser.addAllowedExts("zip");
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

   public void itemHistoryClick(AdapterView<?> parent, View view, int position, long id)
   {
      final ModuleWrapper item = adapter.getItem(position);
      libretroPath = item.getUnderlyingFile().getAbsolutePath();
      libretroName = InstalledCoresFragment.sanitizedLibretroName(libretroPath);

      // Show Content History
      //
      final SharedPreferences prefs = PreferenceManager.getDefaultSharedPreferences(this);
      final String defaultConfig = UserPreferences.defaultBaseDir + "/config";
      String cfgDir = prefs.getBoolean("config_directory_enable", false) ?
            prefs.getString("rgui_config_directory", defaultConfig) : defaultConfig;
      String historyPath = cfgDir + '/' + libretroName + '/'
            + libretroName + "_history.txt";

      String title = "History (" + item.getCoreTitle() + ")";
      DirectoryFragment contentBrowser = DirectoryFragment.newInstance(title,
            item.getSupportedExtensions());

      // No history if no supported file extensions
      if (contentBrowser == null)
      {
         DarkToast.makeText(this, String.format(getString(R.string.contentless_core),
               item.getCoreTitle()));
         return;
      }

      contentBrowser.setShowMameTitles(prefs.getBoolean("mame_titles", false));
      contentBrowser.setFileListPath(historyPath);
      contentBrowser.setOnDirectoryFragmentClosedListener(this);
      contentBrowser.show(getSupportFragmentManager(), "contentBrowser");
   }
   
   private final AdapterView.OnItemClickListener onClickListener = new AdapterView.OnItemClickListener()
   {
      @Override
      public void onItemClick(AdapterView<?> listView, View view, int position, long id)
      {
         itemClick(listView, view, position, id);
      }
   };

   @Override
   public void onCreateContextMenu(ContextMenu menu, View v, ContextMenu.ContextMenuInfo menuInfo)
   {
      super.onCreateContextMenu(menu, v, menuInfo);

      MenuInflater inflater = this.getMenuInflater();
      inflater.inflate(R.menu.launcher_context_menu, menu);
   }

   @Override
   public boolean onContextItemSelected(MenuItem item)
   {
      final AdapterView.AdapterContextMenuInfo info = (AdapterView.AdapterContextMenuInfo)item.getMenuInfo();

      switch (item.getItemId())
      {
         case R.id.show_history:
         {
            itemHistoryClick(null, null, info.position, 0);
            return true;
         }
         default:
            return super.onContextItemSelected(item);
      }
   }
   
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

   public static void clearTemporaryStorage(Context ctx)
   {
      File tmpDir = new File(ctx.getApplicationInfo().dataDir, "tmp");
      if (tmpDir.isDirectory())
      {
         String[] names = tmpDir.list();
         for (String name : names)
            NativeInterface.DeleteDirTree(new File(tmpDir, name));
      }
   }

   @Override
   public void onDirectoryFragmentClosed(String path)
   {
      SharedPreferences prefs = UserPreferences.getPreferences(this);

      if (!path.isEmpty())
      {
         int lastSlash = path.lastIndexOf('/');

         if (!new File(path).exists())
         {
            DarkToast.makeText(this, String.format(getString(R.string.file_dne),
                  path.substring(lastSlash + 1)));
            return;
         }

         prefs.edit().putString(libretroName + "_directory", path.substring(0, lastSlash)).apply();

         DarkToast.makeText(this, String.format(getString(R.string.loading_data),
               path.substring(lastSlash + 1)));
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
      if (prefs.getBoolean("exit_to_launcher", false))
         retro.putExtra("EXITTOAPPID", getString(R.string.app_id));

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