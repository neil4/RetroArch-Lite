package com.retroarch.browser.coremanager.fragments;

import android.content.Context;
import android.graphics.Point;
import android.graphics.Typeface;
import android.os.Bundle;
import android.support.v4.app.DialogFragment;
import android.view.Display;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.view.WindowManager;
import android.widget.ArrayAdapter;
import android.widget.ListView;
import android.widget.TextView;

import com.retroarch.browser.ModuleWrapper;
import com.retroarchlite.R;

import java.io.File;

/**
 * Fragment that displays information about a selected core.
 */
public final class InstalledCoreInfoFragment extends DialogFragment
{
   /**
    * Creates a new instance of a InstalledCoreInfoFragment.
    * 
    * @param core The wrapped core to represent.
    * 
    * @return a new instance of a InstalledCoreInfoFragment.
    */
   public static InstalledCoreInfoFragment newInstance(ModuleWrapper core)
   {
      InstalledCoreInfoFragment cif = new InstalledCoreInfoFragment();

      // Set the core path as an argument.
      // This will allow us to re-retrieve information if the Fragment
      // is destroyed upon state changes
      Bundle args = new Bundle();
      args.putString("core_path", core.getUnderlyingFile().getPath());
      cif.setArguments(args);

      return cif;
   }

   @Override
   public View onCreateView(LayoutInflater inflater, ViewGroup container, Bundle savedInstanceState)
   {
      // Inflate the view.
      ListView infoView = (ListView) inflater.inflate(R.layout.coremanager_listview, container, false);

      // Get the appropriate info providers.
      final Bundle args = getArguments();
      final ModuleWrapper core = new ModuleWrapper(getActivity(), new File(args.getString("core_path")), false);

      // Initialize the core info.
      CoreInfoAdapter adapter = new CoreInfoAdapter(getActivity(), android.R.layout.simple_list_item_2);
      adapter.add(new InstalledCoreInfoItem(getString(R.string.core_info_displayNameTitle),  core.getDisplayName()));
      adapter.add(new InstalledCoreInfoItem(getString(R.string.core_info_systemNameTitle),   core.getEmulatedSystemName()));
      adapter.add(new InstalledCoreInfoItem(getString(R.string.core_info_manufacturer),      core.getManufacturer()));
      adapter.add(new InstalledCoreInfoItem(getString(R.string.core_info_author),            core.getAuthors()));
      adapter.add(new InstalledCoreInfoItem(getString(R.string.core_info_licenseTitle),      core.getCoreLicense()));
      adapter.add(new InstalledCoreInfoItem(getString(R.string.core_info_permissions),       core.getPermissions()));
      adapter.add(new InstalledCoreInfoItem(getString(R.string.core_info_extensions),        core.getCoreSupportedExtensions()));
      adapter.add(new InstalledCoreInfoItem(getString(R.string.core_info_required_hw_api),   core.getCoreRequiredHwApi()));
      adapter.add(new InstalledCoreInfoItem(getString(R.string.core_info_firmwares),         core.getCoreFirmwares()));
      adapter.add(new InstalledCoreInfoItem(getString(R.string.core_info_notes),             core.getCoreNotes()));

      // Add title
      TextView titleView = new TextView(getContext());
      titleView.setText(core.getInternalName());
      titleView.setTextSize(24);
      titleView.setTypeface(Typeface.DEFAULT_BOLD);
      titleView.setPadding(50,50,50,50);
      infoView.addHeaderView(titleView);
      
      // Set the list adapter.
      infoView.setAdapter(adapter);

      return infoView;
   }
   
   @Override
   public void onResume()
   {
      super.onResume();
      final Display display = getActivity().getWindowManager().getDefaultDisplay();
      Point size = new Point();
      display.getSize(size);

      WindowManager.LayoutParams params = getDialog().getWindow().getAttributes();
      if (size.x > size.y)
         params.width = (50 * size.x) / 100;

      getDialog().getWindow().setAttributes(params);
   }

   /**
    * Adapter backing this InstalledCoreInfoFragment
    */
   private final class CoreInfoAdapter extends ArrayAdapter<InstalledCoreInfoItem>
   {
      private final Context context;
      private final int resourceId;

      /**
       * Constructor
       * 
       * @param context    The current {@link Context}.
       * @param resourceId The resource ID for a layout file containing a layout to use when instantiating views.
       */
      public CoreInfoAdapter(Context context, int resourceId)
      {
         super(context, resourceId);

         this.context = context;
         this.resourceId = resourceId;
      }

      @Override
      public View getView(int position, View convertView, ViewGroup parent)
      {
         if (convertView == null)
         {
            LayoutInflater vi = LayoutInflater.from(context);
            convertView = vi.inflate(resourceId, parent, false);
         }

         final InstalledCoreInfoItem item = getItem(position);
         if (item != null)
         {
            final TextView title    = (TextView) convertView.findViewById(android.R.id.text1);
            final TextView subtitle = (TextView) convertView.findViewById(android.R.id.text2);

            if (title != null)
            {
               title.setText(item.getTitle());
            }

            if (subtitle != null)
            {
               subtitle.setText(item.getSubtitle());
            }
         }

         return convertView;
      }
   }
}
