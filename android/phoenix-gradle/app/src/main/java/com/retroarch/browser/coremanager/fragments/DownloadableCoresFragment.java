package com.retroarch.browser.coremanager.fragments;

import android.app.AlertDialog;
import android.app.ProgressDialog;
import android.content.Context;
import android.content.DialogInterface;
import android.content.Intent;
import android.content.SharedPreferences;
import android.net.Uri;
import android.os.AsyncTask;
import android.os.Build;
import android.os.Bundle;
import android.support.v4.app.ListFragment;
import android.util.Log;
import android.util.Pair;
import android.view.ContextMenu;
import android.view.ContextMenu.ContextMenuInfo;
import android.view.LayoutInflater;
import android.view.MenuInflater;
import android.view.MenuItem;
import android.view.View;
import android.view.ViewGroup;
import android.widget.AdapterView.AdapterContextMenuInfo;
import android.widget.ListView;
import android.widget.Toast;

import com.retroarch.browser.preferences.util.UserPreferences;
import com.retroarchlite.BuildConfig;
import com.retroarchlite.R;

import java.io.BufferedInputStream;
import java.io.BufferedReader;
import java.io.File;
import java.io.FileInputStream;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.io.InputStreamReader;
import java.io.OutputStream;
import java.net.HttpURLConnection;
import java.net.URL;
import java.util.ArrayList;
import java.util.Collections;
import java.util.zip.ZipEntry;
import java.util.zip.ZipInputStream;

import static com.retroarch.browser.coremanager.CoreManagerActivity.getTitlePair;

/**
 * {@link ListFragment} that is responsible for showing
 * cores that are able to be downloaded or are not installed.
 */
public final class DownloadableCoresFragment extends ListFragment
{
   // List of TODOs.
   // - Eventually make the core downloader capable of directory-based browsing from the base URL.
   // - Allow for 'repository'-like core downloading.
   // - Clean this up a little better. It can likely be way more organized.
   // - Use a loading wheel when core retrieval is being done. User may think something went wrong otherwise.
   // - Should probably display a dialog or a toast message when the Internet connection process fails.

   /**
    * Dictates what actions will occur when a core download completes.
    * <p>
    * Acts like a callback so that communication between fragments is possible.
    */
   public interface OnCoreDownloadedListener
   {
      /** The action that will occur when a core is successfully downloaded. */
      void onCoreDownloaded();
   }

   public static final String BUILDBOT_BASE_URL = "http://buildbot.libretro.com";
   public static String BUILDBOT_CORE_URL_ARM = BUILDBOT_BASE_URL + "/nightly/android/latest/armeabi-v7a/";
   public static String BUILDBOT_CORE_URL_INTEL = BUILDBOT_BASE_URL + "/nightly/android/latest/x86/";
   public static final String BUILDBOT_INFO_URL = BUILDBOT_BASE_URL + "/assets/frontend/info.zip";
   
   protected static OnCoreDownloadedListener coreDownloadedListener = null;
   public static DownloadableCoresAdapter sAdapter = null;
   private static boolean InfoFileMissing = false;
   private static boolean InfoDownloadAttempted = false;
   
   @Override
   public View onCreateView(LayoutInflater inflater, ViewGroup container, Bundle savedInstanceState)
   {
      super.onCreateView(inflater, container, savedInstanceState);
      final ListView coreList = (ListView) inflater.inflate(R.layout.coremanager_listview, container, false);
      registerForContextMenu(coreList);

      if (BuildConfig.APPLICATION_ID.contains("64"))
      {
         BUILDBOT_CORE_URL_ARM = BUILDBOT_CORE_URL_ARM.replace("armeabi-v7a","arm64-v8a");
         BUILDBOT_CORE_URL_INTEL = BUILDBOT_CORE_URL_INTEL.replace("x86", "x86_64");
      }

      boolean newAdapter = false;
      if (sAdapter == null)
      {
         sAdapter = new DownloadableCoresAdapter(getActivity(), android.R.layout.simple_list_item_2);
         sAdapter.setNotifyOnChange(true);
         newAdapter = true;
      }
      coreList.setAdapter(sAdapter);
      
      SharedPreferences prefs = UserPreferences.getPreferences(getContext());
      DownloadableCore.sortBySystem = prefs.getBoolean("SortDownloadableCoresBySystem", true);
      
      if (newAdapter)
         new PopulateCoresListOperation(sAdapter).execute();
      
      coreDownloadedListener = (OnCoreDownloadedListener) getActivity();
      
      return coreList;
   }
   
   @Override
   public void onListItemClick(final ListView lv, final View v, final int position, final long id)
   {
      super.onListItemClick(lv, v, position, id);
      final DownloadableCore core = (DownloadableCore) lv.getItemAtPosition(position);

      // Prompt the user for confirmation on downloading the core.
      AlertDialog.Builder notification = new AlertDialog.Builder(getActivity());
      notification.setMessage(String.format(getString(R.string.download_core_confirm_msg), core.getCoreName()));
      notification.setTitle(R.string.confirm_title);
      notification.setNegativeButton(R.string.no, null);
      notification.setPositiveButton(R.string.yes, new DialogInterface.OnClickListener() {
         @Override
         public void onClick(DialogInterface dialog, int which)
         {
            // Begin downloading the core.
            DownloadCore(getActivity(), core);
         }
      });
      notification.show();
   }
   
   protected void DownloadCore(Context ctx, DownloadableCore core)
   {
      new DownloadCoreOperation(ctx, core.getCoreName()).execute(core.getCoreURL(), core.getShortURLName());
   }

   @Override
   public void onCreateContextMenu(ContextMenu menu, View v, ContextMenuInfo menuInfo)
   {
      super.onCreateContextMenu(menu, v, menuInfo);
      
      menu.setHeaderTitle(R.string.downloadable_cores_ctx_title);

      MenuInflater inflater = getActivity().getMenuInflater();
      inflater.inflate(R.menu.downloadable_cores_context_menu, menu);
   }

   @Override
   public boolean onContextItemSelected(MenuItem item)
   {
      final AdapterContextMenuInfo info = (AdapterContextMenuInfo)item.getMenuInfo();

      switch (item.getItemId())
      {
         case R.id.go_to_libretrodocs_ctx_item:
         {
            // todo: Docs pages have no naming convention, so maybe this shouldn't be a feature
            String coreUrlPart = ((DownloadableCore)getListView().getItemAtPosition(info.position))
                  .getCoreName().toLowerCase()
                  .replace(' ', '_')
                  .replace('-', '_')
                  .replace('.', '_')
                  .replace(",", "")
                  .replace("+", "plus")
                  .replace("_git", "")
                  .replace("_svn", "")
                  .replaceFirst(".+/", "")
                  .replaceFirst("_\\(.+\\)", "");
            if (coreUrlPart.matches(".+_[0-9]+_.+"))
               coreUrlPart = coreUrlPart.replaceFirst("_", "");
            Intent browserIntent = new Intent(Intent.ACTION_VIEW,
                  Uri.parse("http://docs.libretro.com/library/" + coreUrlPart));
            startActivity(browserIntent);
            return true;
         }
         case R.id.sort_by_name:
         {
            DownloadableCore.sortBySystem = false;
            reSortCores();
            return true;
         }
         case R.id.sort_by_system:
         {
            DownloadableCore.sortBySystem = true;
            reSortCores();
            return true;
         }
         
         default:
            return super.onContextItemSelected(item);
      }
   }
   
   public void reSortCores()
   {
      if (sAdapter == null || getContext() == null)
         return;

      final ArrayList<DownloadableCore> cores = new ArrayList<>();
      for (int i = 0; i < sAdapter.getCount(); i++)
      {
         DownloadableCore item = sAdapter.getItem(i);

         // Assume .info was flagged missing & then downloaded if systemName empty.
         if (!item.getSystemName().isEmpty())
            cores.add(item);
         else
         {
            final String infoPath = getContext().getApplicationInfo().dataDir + "/info/"
                  + item.getCoreName() + ".info";

            Pair<String,String> pair = getTitlePair(infoPath); // (name,mfr+system)
            cores.add(new DownloadableCore(pair.first, pair.second, item.getCoreURL()));
         }
      }
      Collections.sort(cores);
      sAdapter.clear();
      sAdapter.addAll(cores);
      
      SharedPreferences.Editor editor = UserPreferences.getPreferences(getContext()).edit();
      editor.putBoolean("SortDownloadableCoresBySystem", DownloadableCore.sortBySystem);
      editor.apply();
   }

   protected void downloadInfoFiles()
   {
      try
      {
         new DownloadCoreOperation(getContext(), "info.zip")
               .execute(BUILDBOT_INFO_URL, "info.zip");
      }
      catch (Exception ignored) {}
      InfoDownloadAttempted = true;
   }

   // Async event responsible for populating the Downloadable Cores list.
   private final class PopulateCoresListOperation extends AsyncTask<Void, Void, ArrayList<DownloadableCore>>
   {
      // Acts as an object reference to an adapter in a list view.
      private final DownloadableCoresAdapter adapter;

      /**
       * Constructor
       *
       * @param adapter The adapter to asynchronously update.
       */
      public PopulateCoresListOperation(DownloadableCoresAdapter adapter)
      {
         this.adapter = adapter;
      }

      @Override
      protected ArrayList<DownloadableCore> doInBackground(Void... params)
      {
         final ArrayList<DownloadableCore> downloadableCores = new ArrayList<>();

         try
         {
            String buildbotURL = (Build.CPU_ABI.startsWith("arm") ?
                  BUILDBOT_CORE_URL_ARM : BUILDBOT_CORE_URL_INTEL);

            HttpURLConnection conn = (HttpURLConnection) new URL(buildbotURL + ".index")
                  .openConnection();
            conn.connect();

            if (conn.getResponseCode() != HttpURLConnection.HTTP_OK)
            {
               Log.i("IndexDownload", "HTTP response code not OK. Response code: "
                     + conn.getResponseCode());
               return downloadableCores;
            }

            BufferedReader br = new BufferedReader(new InputStreamReader(conn.getInputStream()));

            for (String fileName; (fileName = br.readLine()) != null;)
            {
               String coreURL  = buildbotURL + fileName;
               String infoPath = getContext().getApplicationInfo().dataDir + "/info/"
                     + fileName.replace("_android.so.zip", ".info");

               Pair<String,String> pair = getTitlePair(infoPath); // (name, mfr+system)
               if (!new File(infoPath).exists())
                  InfoFileMissing = true;

               downloadableCores.add(new DownloadableCore(pair.first, pair.second, coreURL));
            }

            Collections.sort(downloadableCores);
            br.close();
         }
         catch (IOException e)
         {
            Log.e("PopulateCoresListOp", e.toString());
         }

         return downloadableCores;
      }

      @Override
      protected void onPostExecute(ArrayList<DownloadableCore> result)
      {
         super.onPostExecute(result);

         if (result.isEmpty())
            Toast.makeText(adapter.getContext(), R.string.download_core_list_error, Toast.LENGTH_SHORT).show();
         else
         {
            adapter.addAll(result);
            if (InfoFileMissing && !InfoDownloadAttempted)
               downloadInfoFiles();
         }
      }
   }


   public static final class DownloadCoreOperation extends AsyncTask<String, Integer, Void>
   {
      private final ProgressDialog dlg;
      private final Context ctx;
      private final String coreName;
      private boolean overwrite = false;
      private final boolean isInfoZip;
      
      /**
       * Constructor
       *
       * @param ctx      The current {@link Context}.
       * @param coreName The name of the core being downloaded.
       */
      public DownloadCoreOperation(Context ctx, String coreName)
      {
         this.dlg = new ProgressDialog(ctx);
         this.ctx = ctx;
         this.coreName = coreName;
         this.isInfoZip = coreName.equals("info.zip");
      }

      @Override
      protected void onPreExecute()
      {
         super.onPreExecute();

         dlg.setMessage(String.format(ctx.getString(R.string.downloading_msg), coreName));
         dlg.setCancelable(true);
         dlg.setCanceledOnTouchOutside(false);
         dlg.setIndeterminate(false);
         dlg.setMax(100);
         dlg.setProgressStyle(ProgressDialog.STYLE_HORIZONTAL);
         dlg.show();
      }
      
      @Override
      protected Void doInBackground(String... params)
      {
         InputStream is = null;
         OutputStream os = null;
         HttpURLConnection connection = null;
         final File zipPath = new File(ctx.getApplicationInfo().dataDir
               + (isInfoZip ? "/info/" : "/cores/"), params[1]);

         try
         {
            URL url = new URL(params[0]);
            connection = (HttpURLConnection) url.openConnection();
            connection.connect();

            if (connection.getResponseCode() != HttpURLConnection.HTTP_OK)
            {
               Log.i("DownloadCoreOperation", "HTTP response code not OK. Response code: " + connection.getResponseCode());
               return null;
            }

            // Set up the streams
            final int fileLen = connection.getContentLength();
            is = new BufferedInputStream(connection.getInputStream(), 8192);
            os = new FileOutputStream(zipPath);
            
            if (new File(zipPath.getParent(), zipPath.getName().replace(".zip", "")).exists())
               overwrite = true;

            // Download and write to storage.
            long downloaded = 0;
            byte[] buffer = new byte[4096];
            int bufLen;
            while ((bufLen = is.read(buffer)) != -1)
            {
               if (!dlg.isShowing())
               {
                  cancel(true);
                  break;
               }

               downloaded += bufLen;
               if (fileLen > 0)
                  publishProgress((int) (downloaded * 100 / fileLen));

               os.write(buffer, 0, bufLen);
            }

            if (!isCancelled())
               unzipFile(zipPath);
         }
         catch (IOException ignored)
         {
            // Can't really do anything to recover.
         }
         finally
         {
            try
            {
               if (zipPath.exists())
                  zipPath.delete();

               if (os != null)
                  os.close();

               if (is != null)
                  is.close();
            }
            catch (IOException ignored)
            {
            }

            if (connection != null)
               connection.disconnect();
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
    
         if (dlg.isShowing())
            dlg.dismiss();
         
         // Invoke callback to update the installed cores list.
         if (coreDownloadedListener != null)
            coreDownloadedListener.onCoreDownloaded();

         if (!isInfoZip)
            Toast.makeText(this.ctx, this.coreName + (overwrite ? " updated." : " installed."),
                           Toast.LENGTH_LONG).show();
      }

      @Override
      protected void onCancelled(Void result)
      {
         Toast.makeText(this.ctx, this.coreName + " download canceled.",
                        Toast.LENGTH_LONG).show();
      }
   }

   private static void unzipFile(File zipFile)
   {
      ZipInputStream zis = null;

      try
      {
         zis = new ZipInputStream(new FileInputStream(zipFile));
         ZipEntry entry = zis.getNextEntry();

         while (entry != null)
         {
            File file = new File(zipFile.getParent(), entry.getName());

            FileOutputStream fos = new FileOutputStream(file);
            int len;
            byte[] buffer = new byte[4096];
            while ((len = zis.read(buffer)) != -1)
            {
               fos.write(buffer, 0, len);
            }
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
}
