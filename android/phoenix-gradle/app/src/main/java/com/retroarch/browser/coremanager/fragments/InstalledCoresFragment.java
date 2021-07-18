package com.retroarch.browser.coremanager.fragments;

import android.app.AlertDialog;
import android.app.ProgressDialog;
import android.content.Context;
import android.content.DialogInterface;
import android.content.DialogInterface.OnClickListener;
import android.content.SharedPreferences;
import android.os.AsyncTask;
import android.os.Build;
import android.os.Bundle;
import androidx.fragment.app.ListFragment;
import android.view.ContextMenu;
import android.view.ContextMenu.ContextMenuInfo;
import android.view.LayoutInflater;
import android.view.MenuInflater;
import android.view.MenuItem;
import android.view.View;
import android.view.ViewGroup;
import android.widget.AdapterView.AdapterContextMenuInfo;
import android.widget.ArrayAdapter;
import android.widget.ListView;
import android.widget.TextView;
import android.widget.Toast;

import com.retroarch.browser.ModuleWrapper;
import com.retroarch.browser.NativeInterface;
import com.retroarch.browser.preferences.util.UserPreferences;
import com.retroarchlite.BuildConfig;
import com.retroarchlite.R;

import java.io.File;
import java.io.FileInputStream;
import java.io.FileOutputStream;
import java.io.IOException;
import java.util.ArrayList;
import java.util.Collections;
import java.util.List;
import java.util.zip.ZipEntry;
import java.util.zip.ZipOutputStream;

import static com.retroarch.browser.preferences.util.UserPreferences.getPreferences;

/**
 * {@link ListFragment} that displays all of the currently installed cores
 * <p>
 * In terms of layout, this is the fragment that is placed on the
 * left side of the screen within the core manager
 */
public final class InstalledCoresFragment extends ListFragment
{
   public final String BUILDBOT_BASE_URL = "http://buildbot.libretro.com";
   public String BUILDBOT_CORE_URL_ARM = BUILDBOT_BASE_URL + "/nightly/android/latest/armeabi-v7a/";
   public String BUILDBOT_CORE_URL_INTEL = BUILDBOT_BASE_URL + "/nightly/android/latest/x86/";

   // Callback for the interface.
   private OnCoreItemClickedListener callback;

   // Adapter backing this ListFragment.
   private InstalledCoresAdapter adapter;

   /**
    * Interface that a parent fragment must implement
    * in order to display the core info view.
    */
   interface OnCoreItemClickedListener
   {
      /**
       * The action to perform when a core is selected within the list view.
       *
       * @param core     A reference to the actual {@link ModuleWrapper}
       *                 represented by that list item.
       */
      void onCoreItemClicked(ModuleWrapper core);
   }

   @Override
   public void onActivityCreated(Bundle savedInstanceState)
   {
      super.onActivityCreated(savedInstanceState);

      adapter = new InstalledCoresAdapter(getActivity(), android.R.layout.simple_list_item_2, getInstalledCoresList());
      setListAdapter(adapter);

      // Get the callback. (implemented within InstalledCoresManagerFragment).
      callback = (OnCoreItemClickedListener) getParentFragment();
   }
   
   @Override
   public void onViewCreated(View view, Bundle savedInstanceState)
   {
      super.onViewCreated(view, savedInstanceState);

      registerForContextMenu(getListView());

      if (BuildConfig.APPLICATION_ID.contains("64"))
      {
         BUILDBOT_CORE_URL_ARM = BUILDBOT_CORE_URL_ARM.replace("armeabi-v7a","arm64-v8a");
         BUILDBOT_CORE_URL_INTEL = BUILDBOT_CORE_URL_INTEL.replace("x86", "x86_64");
      }
   }
   
   @Override
   public void onListItemClick(ListView l, View v, int position, long id)
   {
      callback.onCoreItemClicked(adapter.getItem(position));

      // Set the item as checked so it highlights in the two-fragment view.
      getListView().setItemChecked(position, true);
   }
   
   @Override
   public void onCreateContextMenu(ContextMenu menu, View v, ContextMenuInfo menuInfo)
   {
      super.onCreateContextMenu(menu, v, menuInfo);

      menu.setHeaderTitle(R.string.installed_cores_ctx_title);

      MenuInflater inflater = getActivity().getMenuInflater();
      inflater.inflate(R.menu.installed_cores_context_menu, menu);
   }

   @Override
   public boolean onContextItemSelected(MenuItem item)
   {
      final AdapterContextMenuInfo info = (AdapterContextMenuInfo)item.getMenuInfo();

      switch (item.getItemId())
      {
         case R.id.update_core:
            return UpdateCore(info.position);
         case R.id.backup_core:
            return BackupCore(info.position);
         case R.id.reset_core_options:
            return DeleteCoreOptions(info.position);
         case R.id.remove_core:
            return RemoveCore(info.position);

         default:
            return super.onContextItemSelected(item);
      }
   }

   /**
    * Refreshes the list of installed cores.
    */
   public void updateInstalledCoresList()
   {
      if (adapter == null)
         return;
      
      adapter.clear();
      for (int i = 0; i < getInstalledCoresList().size(); i++)
      {
         adapter.add(getInstalledCoresList().get(i));
      }
      adapter.notifyDataSetChanged();
   }

   private List<ModuleWrapper> getInstalledCoresList()
   {
      // The list of items that will be added to the adapter backing this ListFragment.
      final List<ModuleWrapper> items = new ArrayList<>();
      
      // Populate the list
      final File[] libs = new File(getActivity().getApplicationInfo().dataDir, "/cores").listFiles();
      for (File lib : libs)
         items.add(new ModuleWrapper(getActivity(), lib, false, false));

      // Sort the list alphabetically
      Collections.sort(items);
      return items;
   }

   // This will be the handler for long clicks on individual list items in this ListFragment.
   public boolean RemoveCore(int position)
   {
      // Begin building the AlertDialog
      final ModuleWrapper item = adapter.getItem(position);
      final AlertDialog.Builder alert = new AlertDialog.Builder(getActivity());
      alert.setTitle(R.string.confirm_title);
      alert.setMessage(String.format(getString(R.string.uninstall_core_message),
            item.getCoreTitle()));
      alert.setNegativeButton(R.string.no, null);
      alert.setPositiveButton(R.string.yes, new OnClickListener()
      {
         @Override
         public void onClick(DialogInterface dialog, int which)
         {
            // Attempt to uninstall the core item.
            if (item.getUnderlyingFile().delete())
            {
               Toast.makeText(getActivity(), String.format(getString(R.string.uninstall_success),
                     item.getCoreTitle()), Toast.LENGTH_LONG).show();
               adapter.remove(item);
               adapter.notifyDataSetChanged();
            }
            else // Failed to uninstall.
            {
               Toast.makeText(getActivity(), String.format(getString(R.string.uninstall_failure),
                     item.getCoreTitle()), Toast.LENGTH_LONG).show();
            }
         }
      });
      alert.show();

      return true;
   }


   /**
    * Removes all option files in core's config directory.
    * @param position list position of current core
    * @return true
    */
   public boolean DeleteCoreOptions(int position)
   {
      // Begin building the AlertDialog
      final ModuleWrapper item = adapter.getItem(position);
      final AlertDialog.Builder alert = new AlertDialog.Builder(getActivity());
      alert.setTitle(R.string.confirm_title);
      alert.setMessage(String.format(getString(R.string.reset_core_options_message),
            item.getCoreTitle()));
      alert.setNegativeButton(R.string.no, null);
      alert.setPositiveButton(R.string.yes, new OnClickListener()
      {
         @Override
         public void onClick(DialogInterface dialog, int which)
         {
            boolean success = true;
            final SharedPreferences prefs = getPreferences(getContext());
            final String defaultConfig = UserPreferences.defaultBaseDir + "/config";

            String cfgDir = prefs.getBoolean("config_directory_enable", false) ?
                  prefs.getString("rgui_config_directory", defaultConfig) : defaultConfig;
            String libretroName = sanitizedLibretroName(item.getUnderlyingFile().getName());
            String coreCfgDirPath = cfgDir + '/' + libretroName;

            File coreCfgDir = new File(coreCfgDirPath);
            if (coreCfgDir.isDirectory())
            {
               String[] names = coreCfgDir.list();
               for (String name : names)
                  if (name.endsWith(".opt"))
                     success &= new File(coreCfgDir, name).delete();
            }

            String fmt = getString(success ?
                  R.string.reset_core_options_success : R.string.reset_core_options_failure);
            Toast.makeText(getActivity(),
                  String.format(fmt, item.getCoreTitle()), Toast.LENGTH_LONG).show();
         }
      });
      alert.show();

      return true;
   }
   

   /**
    * @param path (not modified) relative or absolute core file path
    * @return path without directory or "_libretro_android.so"
    */
   public static String sanitizedLibretroName(String path)
   {
      int startIndex = path.lastIndexOf('/') + 1;

      return path.substring(startIndex, path.indexOf("_libretro", startIndex));
   }

   /**
    * The {@link ArrayAdapter} that backs this InstalledCoresFragment.
    */
   private final class InstalledCoresAdapter extends ArrayAdapter<ModuleWrapper>
   {
      private final Context context;
      private final int resourceId;
      private final List<ModuleWrapper> items;

      /**
       * Constructor
       * 
       * @param context    The current {@link Context}.
       * @param resourceId The resource ID for a layout file containing a layout to use when instantiating views.
       * @param items      The list of items to represent in this adapter.
       */
      public InstalledCoresAdapter(Context context, int resourceId, List<ModuleWrapper> items)
      {
         super(context, resourceId, items);

         this.context = context;
         this.resourceId = resourceId;
         this.items = items;
      }

      @Override
      public ModuleWrapper getItem(int i)
      {
         return items.get(i);
      }

      @Override
      public View getView(int position, View convertView, ViewGroup parent)
      {
         if (convertView == null)
         {
            LayoutInflater vi = LayoutInflater.from(context);
            convertView = vi.inflate(resourceId, parent, false);
         }

         final ModuleWrapper item = items.get(position);
         if (item != null)
         {
            TextView title    = (TextView) convertView.findViewById(android.R.id.text1);
            TextView subtitle = (TextView) convertView.findViewById(android.R.id.text2);

            if (title != null)
            {
               title.setText(item.getText());
            }

            if (subtitle != null)
            {
               subtitle.setText(item.getSubText());
            }
         }

         return convertView;
      }
   }
   
   /**
    * Update core to the latest libretro.com nightly build
    * @param position list position of current core
    * @return true
    */
   public boolean UpdateCore(int position)
   {
      final String coreFileName = adapter.getItem(position).getUnderlyingFile().getName();
      final String coreURL = (Build.CPU_ABI.startsWith("arm") ?
                              BUILDBOT_CORE_URL_ARM : BUILDBOT_CORE_URL_INTEL)
                             + coreFileName.concat(".zip");

      final DownloadableCore core = new DownloadableCore(
            adapter.getItem(position).getCoreTitle(), "", coreURL, false);

      // Prompt the user for confirmation on updating the core.
      AlertDialog.Builder notification = new AlertDialog.Builder(getActivity());
      notification.setMessage(String.format(getString(R.string.update_core_confirm_msg), core.getCoreName()));
      notification.setTitle(R.string.confirm_title);
      notification.setNegativeButton(R.string.no, null);
      notification.setPositiveButton(R.string.yes, new DialogInterface.OnClickListener() {
         @Override
         public void onClick(DialogInterface dialog, int which)
         {
            // Begin downloading the core.
            new DownloadableCoresFragment.DownloadCoreOperation(getContext(), core.getCoreName()).execute(core.getCoreURL(), core.getShortURLName());
         }
      });
      notification.show();
      
      return true;
   }
   
   /**
    * Copy core's .so and .info files to the Backup Core directory
    * @param position list position of current core
    * @return true
    */
   public boolean BackupCore(int position)
   {
      // Begin building the AlertDialog
      final ModuleWrapper item = adapter.getItem(position);
      final AlertDialog.Builder alert = new AlertDialog.Builder(getActivity());
      
      SharedPreferences settings = UserPreferences.getPreferences(getActivity());
      String backupCoresDir = settings.getBoolean("backup_cores_directory_enable", false) ?
              settings.getString("backup_cores_directory", BackupCoresFragment.defaultBackupCoresDir)
              : BackupCoresFragment.defaultBackupCoresDir;
      final String zipPath = backupCoresDir + "/" + item.getUnderlyingFile().getName()
            .replace(".so",".zip");
      final File destFile = new File(zipPath);
      
      if (!destFile.getParentFile().exists())
         destFile.getParentFile().mkdirs();

      alert.setTitle(R.string.confirm_title);
      alert.setMessage(String.format(getString(R.string.backup_core_message)
               + (destFile.exists() ? "\nBackup exists and will be overwritten." : ""),
            item.getCoreTitle()));
      alert.setNegativeButton(R.string.no, null);
      alert.setPositiveButton(R.string.yes, new OnClickListener()
      {
         @Override
         public void onClick(DialogInterface dialog, int which)
         {
            new BackupCoreOperation(getActivity(), item.getCoreTitle())
                .execute(item.getUnderlyingFile().getAbsolutePath(), zipPath);
         }
      });
      alert.show();

      return true;
   }
   
   /**
    * Executed when the user confirms a core backup.
    */
   private final class BackupCoreOperation extends AsyncTask<String, Integer, Void>
   {
      private final ProgressDialog dlg;
      private final Context ctx;
      private final String coreName;

      /**
       * Constructor
       *
       * @param ctx      The current {@link Context}.
       * @param coreName The name of the core being downloaded.
       */
      public BackupCoreOperation(Context ctx, String coreName)
      {
         this.dlg = new ProgressDialog(ctx);
         this.ctx = ctx;
         this.coreName = coreName;
      }

      @Override
      protected void onPreExecute()
      {
         super.onPreExecute();
         
         dlg.setMessage(String.format(ctx.getString(R.string.backing_up_msg), coreName));
         dlg.setCancelable(false);
         dlg.setCanceledOnTouchOutside(false);
         dlg.setIndeterminate(false);
         dlg.setMax(100);
         dlg.setProgressStyle(ProgressDialog.STYLE_HORIZONTAL);
         dlg.show();
      }
      
      @Override
      protected Void doInBackground(String... params)
      {
         String libPath = params[0];
         String zipPath = params[1];
         FileInputStream fis;
         FileOutputStream fos;
         ZipOutputStream zos;
         ZipEntry entry;
         int chunkLen;
         long fileLen;
         long totalRead = 0;

         File libFile = new File(libPath);
         File infoFile = new File(libPath.replace("/cores/", "/info/")
               .replace("_android.so", ".info"));
         fileLen = (int) libFile.length();

         try
         {
            fis = new FileInputStream(libFile);
            fos = new FileOutputStream(new File(zipPath));
            zos = new ZipOutputStream(fos);
            entry = new ZipEntry(libFile.getName());
            byte[] buffer = new byte[8192];

            zos.putNextEntry(entry);
            while((chunkLen = fis.read(buffer)) >= 0)
            {
               zos.write(buffer, 0, chunkLen);
               totalRead += chunkLen;
               publishProgress((int) (totalRead * 100 / fileLen));
            }
            fis.close();

            if (infoFile.exists())
            {
               fis = new FileInputStream(infoFile);
               entry = new ZipEntry(infoFile.getName());

               zos.putNextEntry(entry);
               while((chunkLen = fis.read(buffer)) >= 0)
                  zos.write(buffer, 0, chunkLen);
               fis.close();
            }

            zos.close();
            fos.close();
         }
         catch (IOException ignored)
         {
            // Can't do anything.
         }

         return null;
      }

      @Override
      protected void onProgressUpdate(Integer... progress)
      {
         super.onProgressUpdate(progress);
         dlg.setProgress(progress[0]);
      }

      @Override
      protected void onPostExecute(Void result)
      {
         super.onPostExecute(result);
         dlg.dismiss();
         Toast.makeText(getActivity(), coreName + " backup created.", Toast.LENGTH_LONG).show();
      }
   }
}
