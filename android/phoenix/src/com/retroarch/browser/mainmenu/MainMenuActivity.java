package com.retroarch.browser.mainmenu;

import android.Manifest;
import android.app.Dialog;
import android.content.Intent;
import android.content.SharedPreferences;
import android.content.pm.PackageManager;
import android.content.ComponentName;
import android.media.AudioManager;
import android.os.Bundle;
import android.os.Handler;
import android.preference.PreferenceActivity;
import android.preference.PreferenceManager;
import android.provider.Settings;
import android.support.v4.app.ActivityCompat;
import android.support.v4.app.FragmentActivity;
import android.support.v4.content.ContextCompat;
import android.util.Log;
import android.view.Menu;
import android.view.MenuItem;
import android.view.View;
import android.widget.AdapterView;
import android.widget.ListView;
import android.widget.Toast;
import com.retroarchlite.R;
import com.retroarch.browser.IconAdapter;
import com.retroarch.browser.ModuleWrapper;
import com.retroarch.browser.NativeInterface;
import com.retroarch.browser.coremanager.fragments.InstalledCoresFragment;
import com.retroarch.browser.dirfragment.DirectoryFragment;
import com.retroarch.browser.dirfragment.DirectoryFragment.OnDirectoryFragmentClosedListener;
import com.retroarch.browser.preferences.util.UserPreferences;
import com.retroarch.browser.retroactivity.RetroActivity;
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
 * {@link PreferenceActivity} subclass that provides all of the
 * functionality of the main menu screen.
 */
public final class MainMenuActivity extends FragmentActivity implements OnDirectoryFragmentClosedListener
{
   private static final int REQUEST_DANGEROUS_PERMISSION = 112;
   private IconAdapter<ModuleWrapper> adapter;
   static String libretroPath = "";
   static String libretroName = "";
   static DirectoryFragment contentBrowser;
   static Intent retro = null;
   
   @Override
   public void onCreate(Bundle savedInstanceState)
   {
      super.onCreate(savedInstanceState);

      extractAssets();
      
      // Load the main menu layout
      setContentView(R.layout.line_list);
      createList();

      // Bind audio stream to hardware controls.
      setVolumeControlStream(AudioManager.STREAM_MUSIC);
      
      if (android.os.Build.VERSION.SDK_INT >= 21)
      {
         getPermissions( new String[] {Manifest.permission.WRITE_EXTERNAL_STORAGE,
                                       Manifest.permission.ACCESS_COARSE_LOCATION,
                                       Manifest.permission.ACCESS_FINE_LOCATION,
                                       Manifest.permission.CAMERA});
      }
   }
   
   protected void getPermissions( final String[] permission_strings )
   {
      boolean any_denied = false;
      for ( int i = 0; i < permission_strings.length; i++ )
      {
         int permission_int = ContextCompat.checkSelfPermission(this, permission_strings[i]);

         if (permission_int != PackageManager.PERMISSION_GRANTED) {
            Log.i("RetroArch Lite", permission_strings[i] + " denied");
            any_denied = true;
         }
      }
      
      if (any_denied)
      {
         ActivityCompat.requestPermissions(this,
                    permission_strings,
                    REQUEST_DANGEROUS_PERMISSION);
      }
   }

   public void setCoreTitle(String core_name)
   {
      setTitle("RetroArch : " + core_name);
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
   public boolean onCreateOptionsMenu(Menu aMenu) {
      super.onCreateOptionsMenu(aMenu);
      getMenuInflater().inflate(R.menu.options_menu, aMenu);
      return true;
   }
   
   @Override
   public boolean onOptionsItemSelected(MenuItem aItem) {
      switch (aItem.getItemId()) {
      case R.id.settings:
         Intent rset = new Intent(this, com.retroarch.browser.preferences.PreferenceActivity.class);
         startActivity(rset);
         return true;

      default:
         return super.onOptionsItemSelected(aItem);
      }
   }
   
   public void createList()
   {
      // Inflate the ListView we're using.
      ListView coreList = (ListView) findViewById(R.id.list);

      // Populate 32-bit list
      //
      final List<ModuleWrapper> cores = new ArrayList<ModuleWrapper>();
      File cores_dir32 = new File(getApplicationInfo().dataDir, "cores");
      if (!cores_dir32.exists())
         cores_dir32.mkdir();
      final File[] libs32 = cores_dir32.listFiles();
      
      for (final File lib : libs32)
         cores.add(new ModuleWrapper(this, lib));
      
      // Populate 64-bit list
      //
      String dataDir64 = getApplicationInfo().dataDir.replaceFirst("retroarchlite", "retroarchlite64");
      File cores_dir64 = new File(dataDir64, "cores");
      if (cores_dir64.exists())
      {
         final File[] libs64 = cores_dir64.listFiles();
         for (final File lib : libs64)
            cores.add(new ModuleWrapper(this, lib));
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
      // Load Core
      final ModuleWrapper item = adapter.getItem(position);
      libretroPath = item.getUnderlyingFile().getAbsolutePath();
      libretroName = InstalledCoresFragment.sanitizedLibretroName(libretroPath);

      // Show Content Directory
      //
      final SharedPreferences prefs = PreferenceManager.getDefaultSharedPreferences(this);

      if (!new File(libretroPath).isDirectory())
      {
         contentBrowser = null;  // rebuild contentBrowser here
         contentBrowser = DirectoryFragment.newInstance(item.getText(), item.getSupportedExtensions());
         
         // Assume no content if no file extentions
         if (contentBrowser == null)
         {
            onDirectoryFragmentClosed("");
            return;
         }
         
         contentBrowser.setOnDirectoryFragmentClosedListener(this);

         String startPath = prefs.getString( libretroName + "_directory", "");
         if ( startPath.isEmpty() || new File(startPath).exists() == false )
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

      // Java is fun :)
      Thread assetsThread = new Thread(new Runnable()
      {
         public void run()
         {
            extractAssetsThread();
            handler.post(new Runnable()
            {
               public void run()
               {
                  dialog.dismiss();
               }
            });
         }
      });
      assetsThread.start();

      dialog.show();
      try{ assetsThread.join(); }
      catch (InterruptedException e) {};
   }

   // Extract assets from native code. Doing it from Java side is apparently unbearably slow ...
   private void extractAssetsThread()
   {
      try
      {
         final String dataDir = getApplicationInfo().dataDir;
         final String apk = getApplicationInfo().sourceDir;

         boolean success = NativeInterface.extractArchiveTo(apk, "assets", dataDir);
         if (!success) {
            throw new IOException("Failed to extract assets ...");
         }

         File cacheVersion = new File(dataDir, ".cacheversion");
         DataOutputStream outputCacheVersion = new DataOutputStream(new FileOutputStream(cacheVersion, false));
         outputCacheVersion.writeInt(getVersionCode());
         outputCacheVersion.close();
      }
      catch (IOException e)
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
      SharedPreferences prefs = UserPreferences.getPreferences(this);
      if (!path.isEmpty())
      {
         SharedPreferences.Editor edit = prefs.edit();
         edit.putString( libretroName + "_directory",
                         path.substring(0, path.lastIndexOf( "/" )) ).commit();
      }

      String current_ime = Settings.Secure.getString(getContentResolver(), Settings.Secure.DEFAULT_INPUT_METHOD);
      Toast.makeText(this, String.format(getString(R.string.loading_data), path), Toast.LENGTH_SHORT).show();
      
      boolean usingSharedActivity = false;
      if (libretroPath.contains("retroarchlite64"))
      {
         usingSharedActivity = true;
         retro = new Intent();
         retro.setComponent(new ComponentName("com.retroarchlite64",
                                              "com.retroarch.browser.retroactivity.RetroActivity"));
      }
      else
         retro = new Intent(this, RetroActivity.class);

      if (!path.isEmpty())
         retro.putExtra("ROM", path);
      retro.putExtra("LIBRETRO", libretroPath);
      retro.putExtra("CONFIGFILE", UserPreferences.getDefaultConfigPath(this));
      retro.putExtra("IME", current_ime);
      retro.putExtra("DATADIR", getApplicationInfo().dataDir);

      startActivity(retro);

      if (usingSharedActivity)
         retro = null;
      
   }
   
   @Override
   public void onResume()
   {
      if (retro != null)
         startActivity(retro);
      else
         createList();  // assume a core was added/updated/removed
      super.onResume();
   }

}