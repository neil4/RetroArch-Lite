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
import android.widget.EditText;
import android.widget.ListView;

import com.retroarch.browser.DarkToast;
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

import static android.content.pm.ActivityInfo.SCREEN_ORIENTATION_LOCKED;
import static android.content.pm.ActivityInfo.SCREEN_ORIENTATION_UNSPECIFIED;
import static com.retroarch.browser.coremanager.CoreManagerActivity.getTitlePair;
import static com.retroarch.browser.coremanager.CoreManagerActivity.infoBasename;

/**
 * {@link ListFragment} that is responsible for showing local backup
 * cores that can be installed.
 */
public final class BackupCoresFragment extends ListFragment
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

   private String backupCoresDir;
   public static String defaultBackupCoresDir = UserPreferences.defaultBaseDir + "/cores32";

   private OnCoreCopiedListener coreCopiedListener = null;

   protected DownloadableCoresAdapter adapter = null;
   private SharedPreferences sharedPrefs = null;
   
   @Override
   public View onCreateView(LayoutInflater inflater, ViewGroup container, Bundle savedInstanceState)
   {
      super.onCreateView(inflater, container, savedInstanceState);
      if (BuildConfig.APPLICATION_ID.contains("64"))
         defaultBackupCoresDir = defaultBackupCoresDir.replace("cores32", "cores64");
      sharedPrefs = UserPreferences.getPreferences(getActivity());
      backupCoresDir = sharedPrefs.getBoolean("backup_cores_directory_enable", false) ?
            sharedPrefs.getString("backup_cores_directory", defaultBackupCoresDir) : defaultBackupCoresDir;

      ListView listView = (ListView) inflater.inflate(R.layout.coremanager_listview, container, false);
      registerForContextMenu(listView);

      adapter = new DownloadableCoresAdapter(getActivity(), android.R.layout.simple_list_item_2);
      listView.setAdapter(adapter);

      coreCopiedListener = (OnCoreCopiedListener) getActivity();

      PopulateCoresList();
      
      return listView;
   }

   public void updateList()
   {
      if (adapter != null && sharedPrefs != null)
      {
         backupCoresDir = sharedPrefs.getBoolean("backup_cores_directory_enable", false) ?
               sharedPrefs.getString("backup_cores_directory", defaultBackupCoresDir) : defaultBackupCoresDir;
         PopulateCoresList();
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
            String destDir = getContext().getApplicationInfo().dataDir + "/cores";
            String srcPath = core.getFilePath();

            if (srcPath.endsWith(".zip"))
            {
               new DownloadableCoresFragment.UnzipCoreOperation(
                     getContext(), core.getCoreName(), srcPath, destDir, false).execute();
            }
            else
            {
               new CopyCoreOperation(getActivity(), core).execute(core.getShortURLName());
            }
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

      switch (item.getItemId())
      {
         case R.id.rename_backupcore:
            return RenameFile(info.position);
         case R.id.remove_backupcore:
            return RemoveCore(info.position);
         default:
            return super.onContextItemSelected(item);
      }
   }
   
   void PopulateCoresList()
   {
      try
      {
         final File[] files = new File(backupCoresDir).listFiles();
         final ArrayList<DownloadableCore> cores = new ArrayList<>();

         SharedPreferences prefs = UserPreferences.getPreferences(getContext());
         boolean sortBySys = prefs.getBoolean("sort_cores_by_system", true);

         for (File file : files)
         {
            String urlPath = file.toURI().toString();
            String fileName = file.getName();

            boolean isZip = fileName.endsWith(".zip");
            if (!isZip && !fileName.endsWith(".so"))
               continue;

            String infoName = infoBasename(fileName);
            String infoPath = getContext().getApplicationInfo().dataDir + "/info/" + infoName;
            if (!isZip && !new File(infoPath).exists())
               infoPath = backupCoresDir + '/' + infoName;

            Pair<String, String> pair = getTitlePair(infoPath); // (name,mfr+system)

            cores.add(new DownloadableCore(pair.first, pair.second, urlPath, sortBySys));
         }

         Collections.sort(cores);
         adapter.clear();
         adapter.addAll(cores);
         adapter.notifyDataSetChanged();
      }
      catch (Exception e)
      {
         Log.e("PopulateCoresList", e.toString());
      }
   }

   /**
    * Executed when the user confirms to install an uncompressed backup.
    */
   private final class CopyCoreOperation extends AsyncTask<String, Integer, Void>
   {
      private final ProgressDialog dlg;
      private final Context ctx;
      private final String coreName;
      private final String fileName;

      /**
       * Constructor
       *
       * @param ctx  The current {@link Context}.
       * @param core Object describing the core to copy.
       */
      public CopyCoreOperation(Context ctx, DownloadableCore core)
      {
         this.dlg = new ProgressDialog(ctx);
         this.ctx = ctx;
         this.coreName = core.getCoreName();
         this.fileName = core.getShortURLName();

         // TODO: Handle orientation changes
         getActivity().setRequestedOrientation(SCREEN_ORIENTATION_LOCKED);
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
         dlg.setProgressNumberFormat(null);
         dlg.show();
      }

      @Override
      protected Void doInBackground(String... params)
      {
         InputStream is = null, is2 = null;
         OutputStream os = null, os2 = null;

         try
         {
            // Set up the streams
            File inFile = new File(backupCoresDir, fileName);
            File outFile = new File(ctx.getApplicationInfo().dataDir + "/cores", fileName);
            final long fileLen = inFile.length();
            is = new FileInputStream(inFile);
            os = new FileOutputStream(outFile);

            // Copy core to internal directory
            long copied = 0;
            byte[] buffer = new byte[65536];
            int bufLen;
            while ((bufLen = is.read(buffer)) != -1)
            {
               copied += bufLen;
               publishProgress((int) (copied * 100 / fileLen));
               os.write(buffer, 0, bufLen);
            }

            // Copy info file if there is one
            String infoName = infoBasename((fileName));
            inFile = new File(backupCoresDir, infoName);
            if (infoName.endsWith(".info") && inFile.exists())
            {
               outFile = new File(ctx.getApplicationInfo().dataDir + "/info", infoName);
               is2 = new FileInputStream(inFile);
               os2 = new FileOutputStream(outFile);
               while ((bufLen = is2.read(buffer)) != -1)
                  os2.write(buffer, 0, bufLen);
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

         // Invoke callback to update the installed cores list.
         if (coreCopiedListener != null)
            coreCopiedListener.onCoreCopied();

         if (dlg.isShowing())
            dlg.dismiss();

         DarkToast.makeText(getActivity(),
               (coreName.isEmpty() ? "Core" : coreName) + " installed.");

         getActivity().setRequestedOrientation(SCREEN_ORIENTATION_UNSPECIFIED);
      }
   }

   public boolean RenameFile(int position)
   {
      final DownloadableCore core = adapter.getItem(position);
      final File oldFile   = new File(core.getFilePath());
      final String oldName = oldFile.getName();
      final String ext     = oldName.substring(oldName.lastIndexOf('.'));

      // Create EditText view
      final View dialogView = requireActivity().getLayoutInflater()
            .inflate(R.layout.edit_text_dialog, null);
      final EditText editText = dialogView.findViewById(R.id.edit_line);
      editText.setText(oldName);

      // Build AlertDialog
      final AlertDialog.Builder alert = new AlertDialog.Builder(getContext());
      alert.setView(dialogView)
           .setTitle(R.string.rename_title)
           .setNegativeButton(R.string.cancel, null);
      alert.setPositiveButton(R.string.confirm, new DialogInterface.OnClickListener()
      {
         @Override
         public void onClick(DialogInterface dialog, int which)
         {
            final String input   = editText.getText().toString().trim();
            final String newName = input.endsWith(ext) ? input : input + ext;
            final File newFile   = new File(oldFile.getParentFile(), newName);

            if (newName.equals(oldName) || newName.equals(ext))
               return;
            if (newFile.exists())
               DarkToast.makeText(getActivity(), "A duplicate name exists.");
            else try
            {
               if (oldFile.renameTo(newFile))
               {
                  updateList();
                  DarkToast.makeText(getActivity(), "File renamed.");
               }
               else
                  DarkToast.makeText(getActivity(), "Invalid file name");
            }
            catch (Exception e)
            {
               DarkToast.makeText(getActivity(), "Access denied.");
            }
         }
      });
      // TODO: Handle orientation changes
      getActivity().setRequestedOrientation(SCREEN_ORIENTATION_LOCKED);
      alert.setOnDismissListener(
            dialog -> getActivity().setRequestedOrientation(SCREEN_ORIENTATION_UNSPECIFIED)
      );
      alert.show();

      return true;
   }

   public boolean RemoveCore(int position)
   {
      final DownloadableCore core = adapter.getItem(position);
      final File coreFile   = new File(core.getFilePath());
      final String fileName = coreFile.getName();

      // Begin building the AlertDialog
      final AlertDialog.Builder alert = new AlertDialog.Builder(getActivity());
      alert.setTitle(R.string.confirm_title)
           .setMessage("Remove " + core.getCoreName() + " backup?" + "\n\n" + fileName)
           .setNegativeButton(R.string.no, null);
      alert.setPositiveButton(R.string.yes, new DialogInterface.OnClickListener()
      {
         @Override
         public void onClick(DialogInterface dialog, int which)
         {
            // Attempt to remove backup files
            if (coreFile.delete())
            {
               DarkToast.makeText(getActivity(), "Backup core removed.");
               adapter.remove(core);
               adapter.notifyDataSetChanged();
            }
            else // Failed to delete.
            {
               DarkToast.makeText(getActivity(), "Failed to remove backup core.");
            }
         }
      });
      alert.show();

      return true;
   }
}
