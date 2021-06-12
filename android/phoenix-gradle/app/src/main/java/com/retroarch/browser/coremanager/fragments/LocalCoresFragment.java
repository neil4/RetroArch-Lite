package com.retroarch.browser.coremanager.fragments;

import android.app.AlertDialog;
import android.app.ProgressDialog;
import android.content.Context;
import android.content.DialogInterface;
import android.content.SharedPreferences;
import android.os.AsyncTask;
import android.os.Bundle;
import androidx.fragment.app.ListFragment;
import android.util.Log;
import android.util.Pair;
import android.view.ContextMenu;
import android.view.LayoutInflater;
import android.view.MenuInflater;
import android.view.MenuItem;
import android.view.View;
import android.view.ViewGroup;
import android.widget.AdapterView;
import android.widget.ListView;
import android.widget.Toast;

import com.retroarch.browser.preferences.util.UserPreferences;
import com.retroarchlite.BuildConfig;
import com.retroarchlite.R;

import java.io.File;
import java.io.FileInputStream;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.io.OutputStream;
import java.util.ArrayList;
import java.util.Collections;
import java.util.zip.ZipEntry;
import java.util.zip.ZipInputStream;

import static com.retroarch.browser.coremanager.CoreManagerActivity.getTitlePair;

/**
 * {@link ListFragment} that is responsible for showing local backup
 * cores that can be installed.
 */
public final class LocalCoresFragment extends ListFragment
{
   /**
    * Dictates what actions will occur when a core install completes.
    * <p>
    * Acts like a callback so that communication between fragments is possible.
    */
   public interface OnCoreCopiedListener
   {
      void onCoreCopied();
   }

   private String localCoresDir;
   public static String defaultLocalCoresDir = UserPreferences.defaultBaseDir + "/cores32";

   private OnCoreCopiedListener coreCopiedListener = null;

   protected DownloadableCoresAdapter adapter = null;
   private static SharedPreferences sharedSettings = null;
   
   private static boolean zipHasInfoFile = false;
   
   @Override
   public View onCreateView(LayoutInflater inflater, ViewGroup container, Bundle savedInstanceState)
   {
      super.onCreateView(inflater, container, savedInstanceState);
      if (BuildConfig.APPLICATION_ID.contains("64"))
         defaultLocalCoresDir = defaultLocalCoresDir.replace("cores32", "cores64");
      sharedSettings = UserPreferences.getPreferences(getActivity());
      localCoresDir = sharedSettings.getBoolean("backup_cores_directory_enable", false) ?
            sharedSettings.getString("backup_cores_directory", defaultLocalCoresDir) : defaultLocalCoresDir;

      ListView coreList = (ListView) inflater.inflate(R.layout.coremanager_listview, container, false);
      registerForContextMenu(coreList);

      adapter = new DownloadableCoresAdapter(getActivity(), android.R.layout.simple_list_item_2);
      coreList.setAdapter(adapter);

      coreCopiedListener = (OnCoreCopiedListener) getActivity();

      PopulateCoresList();
      
      return coreList;
   }

   public void updateList()
   {
      if (adapter != null && sharedSettings != null)
      {
         localCoresDir = sharedSettings.getBoolean("backup_cores_directory_enable", false) ?
               sharedSettings.getString("backup_cores_directory", defaultLocalCoresDir) : defaultLocalCoresDir;
         PopulateCoresList();
         adapter.notifyDataSetChanged();
      }
   }
   
   @Override
   public void onDestroy()
   {
      super.onDestroy();
   }
   
   @Override
   public void onListItemClick(final ListView lv, final View v, final int position, final long id)
   {
      super.onListItemClick(lv, v, position, id);
      final DownloadableCore core = (DownloadableCore) lv.getItemAtPosition(position);

      // Prompt the user for confirmation on installing the core.
      AlertDialog.Builder notification = new AlertDialog.Builder(getActivity());
      notification.setMessage(String.format(getString(R.string.backup_cores_confirm_install_msg),
                              (core.getCoreName().isEmpty() ? "this core" : core.getCoreName()) ));
      notification.setTitle(R.string.confirm_title);
      notification.setNegativeButton(R.string.no, null);
      notification.setPositiveButton(R.string.yes, new DialogInterface.OnClickListener() {
         @Override
         public void onClick(DialogInterface dialog, int which)
         {
            // Begin copying the core.
            new CopyCoreOperation(getActivity(), core.getCoreName()).execute(core.getShortURLName());
         }
      });
      notification.show();
   }
   
   @Override
   public void onCreateContextMenu(ContextMenu menu, View v, ContextMenu.ContextMenuInfo menuInfo)
   {
      super.onCreateContextMenu(menu, v, menuInfo);

      menu.setHeaderTitle(R.string.backup_cores_ctx_title);

      MenuInflater inflater = getActivity().getMenuInflater();
      inflater.inflate(R.menu.backup_cores_context_menu, menu);
   }

   @Override
   public boolean onContextItemSelected(MenuItem item)
   {
      final AdapterView.AdapterContextMenuInfo info = (AdapterView.AdapterContextMenuInfo)item.getMenuInfo();

      if (item.getItemId() == R.id.remove_localcore)
      {
         return RemoveCore(info.position);
      }
      return super.onContextItemSelected(item);
   }
   
   void PopulateCoresList()
   {
      try
      {
         final File[] files = new File(localCoresDir).listFiles();
         final ArrayList<DownloadableCore> cores = new ArrayList<>();

         for (File file : files)
         {
            String urlPath = file.toURI().toURL().toString();
            String coreName = file.getName();

            boolean isZip = coreName.endsWith(".zip");
            if (!isZip && !coreName.endsWith(".so"))
               continue;

            // Allow any name ending in .so or .zip
            String searchStr = (coreName.contains("_android.") ? "_android" : "")
                  + (isZip ? (coreName.contains(".so.") ? ".so.zip" : ".zip") : ".so");
            String infoPath = localCoresDir + "/" + coreName.replace(searchStr, ".info");

            if (!new File(infoPath).exists())
               infoPath = getContext().getApplicationInfo().dataDir
                     + "/info/" + coreName.replace(searchStr, ".info");

            Pair<String, String> pair = getTitlePair(infoPath); // (name,mfr+system)

            cores.add(new DownloadableCore(pair.first, pair.second, urlPath, false));
         }

         Collections.sort(cores);
         adapter.clear();
         adapter.addAll(cores);
      }
      catch (Exception e)
      {
         Log.e("PopulateCoresList", e.toString());
      }
   }


   // Executed when the user confirms a core download.
   private final class CopyCoreOperation extends AsyncTask<String, Integer, Void>
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
      public CopyCoreOperation(Context ctx, String coreName)
      {
         this.dlg = new ProgressDialog(ctx);
         this.ctx = ctx;
         this.coreName = coreName;
      }

      @Override
      protected void onPreExecute()
      {
         super.onPreExecute();

         dlg.setMessage(String.format(ctx.getString(R.string.installing_msg), coreName));
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
         InputStream is = null, is2 = null;
         OutputStream os = null, os2 = null;
         zipHasInfoFile = false;
         
         try
         {
            // Set up the streams
            File inFile = new File(localCoresDir, params[0]);
            File outFile = new File(ctx.getApplicationInfo().dataDir + "/cores", params[0]);
            final long fileLen = inFile.length();
            is = new FileInputStream(inFile);
            os = new FileOutputStream(outFile);

            // Write core to storage.
            //
            long copied = 0;
            byte[] buffer = new byte[4096];
            int bufLen;
            while ((bufLen = is.read(buffer)) != -1)
            {
               copied += bufLen;
               publishProgress((int) (copied * 100 / fileLen));
               os.write(buffer, 0, bufLen);
            }

            if (outFile.toString().endsWith(".zip"))
            {
               unzipCore(outFile);
               outFile.delete();
            }

            if (!zipHasInfoFile)
            { // Install info file from the same directory, if not part of the .zip
               String infoName = params[0].replace("_android.so.zip", ".info")
                                          .replace("_android.so", ".info")
                                          .replace("_android.zip", ".info");
               inFile = new File(localCoresDir, infoName);
               if (infoName.endsWith(".info") && inFile.exists())
               {
                  outFile = new File(ctx.getApplicationInfo().dataDir + "/info", infoName);
                  is2 = new FileInputStream(inFile);
                  os2 = new FileOutputStream(outFile);
                  while ((bufLen = is2.read(buffer)) != -1)
                     os2.write(buffer, 0, bufLen);
               }
            }
         }
         catch (IOException ignored)
         {
            // Can't really do anything to recover.
         }
         finally
         {
            try
            {
               if (os != null)
                  os.close();
               if (is != null)
                  is.close();
               if (os2 != null)
                  os2.close();
               if (is2 != null)
                  is2.close();
            }
            catch (IOException ignored)
            {
            }

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

         // Invoke callback to update the installed cores list.
         if (coreCopiedListener != null)
            coreCopiedListener.onCoreCopied();

         Toast.makeText(getActivity(), (this.coreName.isEmpty() ? "Core" : this.coreName) + " installed.", Toast.LENGTH_LONG).show();
      }
   }

   private static void unzipCore(File zipFile)
   {
      ZipInputStream zis = null;

      try
      {
         zis = new ZipInputStream(new FileInputStream(zipFile));
         ZipEntry entry = zis.getNextEntry();

         while (entry != null)
         {
            File file;
            if (entry.getName().endsWith(".so"))
               file = new File(zipFile.getParent(), entry.getName());
            else if (entry.getName().endsWith(".info"))
            {
               file = new File(zipFile.getParent().replace("/cores", "/info"), entry.getName());
               zipHasInfoFile = true;
            }
            else
            {
               zis.getNextEntry();
               continue;
            }

            FileOutputStream fos = new FileOutputStream(file);
            int len;
            byte[] buffer = new byte[4096];
            while ((len = zis.read(buffer)) != -1)
               fos.write(buffer, 0, len);
            fos.close();

            entry = zis.getNextEntry();
         }
      }
      catch (IOException ignored)
      {
         // Can't do anything.
      }
      finally
      {
         try
         {
            if (zis != null)
            {
               zis.closeEntry();
               zis.close();
            }
         }
         catch (IOException ignored)
         {
            // Can't do anything
         }
      }
   }

   public boolean RemoveCore(int position)
   {
      final DownloadableCore core = adapter.getItem(position);
      final String corePath = core.getCoreURL().replace("file:", "");
      final String corePrefix = corePath.replace("_android.so.zip", "")
                                        .replace("_android.zip", "")
                                        .replace("_android.so", "");
      
      // Begin building the AlertDialog
      final AlertDialog.Builder alert = new AlertDialog.Builder(getActivity());
      alert.setTitle(R.string.confirm_title);
      alert.setMessage("Remove " + core.getCoreName() + " backup?");
      alert.setNegativeButton(R.string.no, null);
      alert.setPositiveButton(R.string.yes, new DialogInterface.OnClickListener()
      {
         @Override
         public void onClick(DialogInterface dialog, int which)
         {
            // Attempt to remove backup files
            if (new File(corePath).delete())
            {
               // remove info file if no other core files are present
               if ( !new File(corePrefix + "_android.so.zip").exists()
                    && !new File(corePrefix + "_android.so").exists()
                    && !new File(corePrefix + "_android.zip").exists() )
               {
                  new File(corePrefix + ".info").delete();
               }
               
               Toast.makeText(getActivity(), "Backup core removed.", Toast.LENGTH_LONG).show();
               adapter.remove(core);
               adapter.notifyDataSetChanged();
            }
            else // Failed to delete.
            {
               Toast.makeText(getActivity(), "Failed to remove backup core.", Toast.LENGTH_LONG).show();
            }
         }
      });
      alert.show();

      return true;
   }
}
