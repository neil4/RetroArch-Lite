package com.retroarch.browser;

import java.io.File;
import java.util.ArrayList;
import java.util.Collections;
import java.util.List;

import android.os.Bundle;
import android.support.v4.app.DialogFragment;
import android.support.v4.app.ListFragment;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.AdapterView;
import android.widget.AdapterView.OnItemClickListener;
import android.widget.ListView;

import com.retroarchlite.R;
import com.retroarch.browser.mainmenu.MainMenuActivity;

/**
 * {@link ListFragment} subclass that displays the list
 * of selectable cores for emulating games.
 */
public final class CoreSelection extends DialogFragment
{
   private IconAdapter<ModuleWrapper> adapter;

   /**
    * Creates a statically instantiated instance of CoreSelection.
    * 
    * @return a statically instantiated instance of CoreSelection.
    */
   public static CoreSelection newInstance()
   {
      return new CoreSelection();
   }

   @Override
   public View onCreateView(LayoutInflater inflater, ViewGroup container, Bundle savedInstanceState)
   {
      // Inflate the ListView we're using.
      final ListView coreList = (ListView) inflater.inflate(R.layout.line_list, container, false);
      coreList.setOnItemClickListener(onClickListener);

      // Set the title of the dialog
      getDialog().setTitle(R.string.select_libretro_core);

      // Populate the list
      final List<ModuleWrapper> cores = getInstalledCoresList();

      // Initialize the IconAdapter with the list of cores.
      adapter = new IconAdapter<ModuleWrapper>(getActivity(), R.layout.line_list_item, cores);
      coreList.setAdapter(adapter);

      return coreList;
   }
   
   private List<ModuleWrapper> getInstalledCoresList()
   {
      final List<ModuleWrapper> cores = new ArrayList<ModuleWrapper>();
      final File[] libs = new File(getActivity().getApplicationInfo().dataDir, "cores").listFiles();
      
      for (final File lib : libs)
         cores.add(new ModuleWrapper(getActivity(), lib));

      // Sort the list of cores alphabetically
      Collections.sort(cores);
      
      return cores;
   }
   
   private final OnItemClickListener onClickListener = new OnItemClickListener()
   {
      @Override
      public void onItemClick(AdapterView<?> listView, View view, int position, long id)
      {
         final ModuleWrapper item = adapter.getItem(position);
         ((MainMenuActivity)getActivity()).setCoreTitle(item.getText());
         dismiss();
      }
   };
}
