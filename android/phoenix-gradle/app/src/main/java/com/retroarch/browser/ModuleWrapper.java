package com.retroarch.browser;

import com.retroarch.browser.preferences.util.ConfigFile;

import java.io.File;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.List;

import android.content.Context;
import android.graphics.drawable.Drawable;
import android.content.SharedPreferences;
import android.os.Environment;
import com.retroarch.browser.preferences.util.UserPreferences;

import com.retroarchlite.R;

/**
 * Wrapper class that encapsulates a libretro core 
 * along with information about said core.
 */
public final class ModuleWrapper implements IconAdapterItem, Comparable<ModuleWrapper>
{
   private final File file;
   private final String displayName;
   private String coreName;
   private final List<String> manufacturer;
   private final String systemName;
   private final List<String> license;
   private final String notes;
   private final List<String> authors;
   private final List<String> supportedExtensions;
   private final List<String> permissions;
   private final List<String> requiredHwApi;
   private boolean supportsNoGame;
   private int firmwareCount = 0;
   private String firmwares;
   private boolean is64bit;
   private boolean showAbi;

   /**
    * Constructor
    * 
    * @param context The current {@link Context}.
    * @param file    The {@link File} instance of the core being wrapped.
    */
   public ModuleWrapper(Context context, File file, boolean showAbi)
   {
      this.file = file;
      SharedPreferences prefs = UserPreferences.getPreferences(context.getApplicationContext());
      is64bit = file.getAbsolutePath().contains("/com.retroarchlite64/");
      this.showAbi = showAbi;

      // Attempt to get the core's info file: dataDir/info/[core name].info

      // Trim platform suffix and extension
      final boolean isValidCoreName = (file.getName().lastIndexOf("_android.so") != -1); 
      final String coreName = (isValidCoreName) ? file.getName().substring(0, file.getName().lastIndexOf("_android.so"))
                                                : file.getName();

      String infoFileDir = context.getApplicationInfo().dataDir + "/info";
      
      // Fix info path for shared cores
      final String sharedId = context.getString(R.string.shared_app_id);
      if (file.getPath().contains(sharedId+'/'))
      {
         if (sharedId.endsWith("64"))
            infoFileDir = infoFileDir.replaceFirst("retroarchlite/", "retroarchlite64/");
         else
            infoFileDir = infoFileDir.replaceFirst("retroarchlite64/", "retroarchlite/");
      }

      // Based on the trimmed core name, get the core info file. Read as config file
      final String infoFilePath = infoFileDir + File.separator + coreName + ".info";
      if (new File(infoFilePath).exists())
      {
         final ConfigFile infoFile = new ConfigFile(infoFilePath);
   
         // Now read info out of the info file. Make them an empty string if the key doesn't exist.
         this.displayName  = (infoFile.keyExists("display_name")) ? infoFile.getString("display_name") : "N/A";
         this.coreName     = (infoFile.keyExists("corename"))     ? infoFile.getString("corename")     : "N/A";
         this.systemName   = (infoFile.keyExists("systemname"))   ? infoFile.getString("systemname")   : "N/A";
         this.notes        = (infoFile.keyExists("notes"))
                             ? infoFile.getString("notes").replace("|", "\n")
                             : "N/A";
         this.firmwareCount = (infoFile.keyExists("firmware_count") ? infoFile.getInt("firmware_count") : 0);
         
         // For manufacturer, licenses, extensions and authors, use '|' delimiter
         final String manufacturer = infoFile.getString("manufacturer");
         if (manufacturer != null && manufacturer.contains("|"))
         {
            this.manufacturer = new ArrayList<String>(Arrays.asList(manufacturer.split("\\|")));
         }
         else
         {
            this.manufacturer = new ArrayList<String>();
            this.manufacturer.add(manufacturer != null ? manufacturer : "N/A");
         }

         final String licenses = infoFile.getString("license");
         if (licenses != null && licenses.contains("|"))
         {
            this.license = new ArrayList<String>(Arrays.asList(licenses.split("\\|")));
         }
         else
         {
            this.license = new ArrayList<String>();
            this.license.add(licenses != null ? licenses : "None");
         }

         final String supportedExts = infoFile.getString("supported_extensions");
         if (supportedExts != null && supportedExts.contains("|"))
         {
            this.supportedExtensions = new ArrayList<String>(Arrays.asList(supportedExts.split("\\|")));
         }
         else
         {
            this.supportedExtensions = new ArrayList<String>();
            this.supportedExtensions.add(supportedExts);
         }

         final String supportsNoGame = infoFile.getString("supports_no_game");
         this.supportsNoGame = supportsNoGame.equalsIgnoreCase("true") ? true : false;

         final String emuAuthors = infoFile.getString("authors");
         if (emuAuthors != null && emuAuthors.contains("|"))
         {
            this.authors = new ArrayList<String>(Arrays.asList(emuAuthors.split("\\|")));
         }
         else
         {
            this.authors = new ArrayList<String>();
            this.authors.add(emuAuthors != null ? emuAuthors : "Unknown");
         }
         
         final String permissions = infoFile.getString("permissions");
         if (permissions != null && permissions.contains("|"))
         {
            this.permissions = new ArrayList<String>(Arrays.asList(permissions.split("\\|")));
         }
         else
         {
            this.permissions = new ArrayList<String>();
            this.permissions.add(permissions != null ? permissions : "None");
         }

         final String requiredHwApi = infoFile.getString("required_hw_api");
         if (requiredHwApi != null && requiredHwApi.contains("|"))
         {
            this.requiredHwApi = new ArrayList<String>(Arrays.asList(requiredHwApi.split("\\|")));
         }
         else
         {
            this.requiredHwApi = new ArrayList<String>();
            this.requiredHwApi.add(requiredHwApi != null ? requiredHwApi : "N/A");
         }
         
         // Firmware list
         //
         if (this.firmwareCount == 0)
            this.firmwares = "N/A";
         else
         {
            final String defaultBase = Environment.getExternalStorageDirectory()
                                                  .getAbsolutePath() + "/RetroArchLite";
            final String defaultSys = defaultBase + "/system";
            String sys_dir = prefs.getBoolean("system_directory_enable", false) ?
                             prefs.getString("system_directory", defaultSys) : defaultSys;
            StringBuilder sb = new StringBuilder();
            for (int i = 0; i < this.firmwareCount; i++)
            {
               String key = "firmware" + Integer.toString(i) + "_desc";
               sb.append((infoFile.keyExists(key)) ? infoFile.getString(key) : "N/A");

               key = "firmware" + Integer.toString(i) + "_path";
               String rel_path = ((infoFile.keyExists(key)) ? infoFile.getString(key) : "?");
               sb.append("\n   path:  system/" + rel_path);

               key = "firmware" + Integer.toString(i) + "_opt";
               sb.append( "\n   status:  "
                          + (new File(sys_dir + "/" + rel_path).exists() ?
                             "present" : "missing")
                          + ", "
                          + ((infoFile.keyExists(key)) ?
                             (infoFile.getBoolean(key) ? "required" : "optional")
                             : "unknown if required") );
               sb.append('\n');
            }
            this.firmwares = sb.toString();
         }
      }
      else // No info file.
      {
         this.displayName = "Unknown";
         this.systemName = "Unknown";
         this.manufacturer = new ArrayList<String>();
         this.manufacturer.add("N/A");
         this.license = new ArrayList<String>();
         this.license.add("Unknown");
         this.notes = "N/A";
         this.authors = new ArrayList<String>();
         this.authors.add("Unknown");
         this.supportedExtensions = new ArrayList<String>();
         this.coreName = coreName;
         this.requiredHwApi = new ArrayList<String>();
         this.requiredHwApi.add("Unknown");
         this.firmwares = "Unknown";
         this.permissions = new ArrayList<String>();
         this.permissions.add("Unknown");
      }
   }

   /**
    * Same as the original constructor, but allows for string paths.
    * 
    * @param context The current {@link Context}.
    * @param path    Path to the file to encapsulate.
    */
   public ModuleWrapper(Context context, String path, boolean showAbi)
   {
      this(context, new File(path), showAbi);
   }

   /**
    * Gets the underlying {@link File} instance for this ModuleWrapper.
    * 
    * @return the underlying {@link File} instance for this ModuleWrapper.
    */
   public File getUnderlyingFile()
   {
      return file;
   }

   /**
    * Gets the display name for this wrapped core.
    * 
    * @return the display name for this wrapped core.
    */
   public String getDisplayName()
   {
      return displayName;
   }

   /**
    * Gets the internal core name for this wrapped core.
    * 
    * @return the internal core name for this wrapped core.
    */
   public String getInternalName()
   {
      return coreName;
   }

   /**
    * Gets the name of the system that is emulated by this wrapped core.
    * (optional - in case core is an emulator)
    * 
    * @return the name of the system that is emulated by this wrapped core.
    */
   public String getEmulatedSystemName()
   {
      return systemName;
   }

   /**
    * Gets the licenses this core is protected under.
    * 
    * @return the licenses this core is protected under.
    */
   public List<String> getCoreLicense()
   {
      return license;
   }
   
   /**
    * Gets list of required firmware files with status
    * @return firmware list
    */
   public String getCoreFirmwares()
   {
      return firmwares;
   }
   
   /**
    * Gets author notes for using this core.
    * 
    * @return notes for running the core.
    */
   public String getCoreNotes()
   {
      return notes;
   }
   
   /**
    * Gets supported file extensions for this core.
    * 
    * @return supported file extensions for this core
    */
   public List<String> getCoreSupportedExtensions()
   {
      return supportedExtensions;
   }

   /**
    * Gets required graphics API for this core.
    *
    * @return required graphics API for this core
    */
   public List<String> getCoreRequiredHwApi()
   {
      return requiredHwApi;
   }

   /**
    * Gets the name of the manufacturer of the console that
    * this core emulates. (optional - in case core is an
    * emulator)
    * 
    * @return the name of the manufacturer of the console that
    *         this core emulates. (optional)
    */
   public List<String> getManufacturer()
   {
      return manufacturer;
   }

   /**
    * Gets the list of authors of this core.
    * 
    * @return the list of authors of this core.
    */
   public List<String> getAuthors()
   {
      return authors;
   }

   /**
    * Gets the list of permissions of this core.
    * 
    * @return the list of permissions of this core.
    */
   public List<String> getPermissions()
   {
      return permissions;
   }

   /**
    * Gets the List of supported extensions for this core.
    * 
    * @return the List of supported extensions for this core.
    */
   public List<String> getSupportedExtensions()
   {
      return supportedExtensions;
   }

   public boolean getSupportsNoGame()
   {
      return supportsNoGame;
   }

   @Override
   public boolean isEnabled()
   {
      return true;
   }

   @Override
   public String getText()
   {
      if (showAbi)
         return bestCoreTitle(displayName, coreName) + (is64bit ? " (64b)" : " (32b)");
      else
         return bestCoreTitle(displayName, coreName);
   }
   
   @Override
   public String getSubText()
   {
      return systemName;
   }

   @Override
   public int getIconResourceId()
   {
      return 0;
   }

   @Override
   public Drawable getIconDrawable()
   {
      return null;
   }

   @Override
   public int compareTo(ModuleWrapper other)
   {
      if(coreName != null)
         return coreName.toLowerCase().compareTo(other.coreName.toLowerCase()); 
      else 
         throw new NullPointerException("The name of this ModuleWrapper is null");
   }

   public static String bestCoreTitle(String displayName, String coreName)
   {
      int i = displayName.indexOf('(');
      int j = displayName.lastIndexOf(')');
      if (i > -1 && j > i+1)
         return displayName.substring(i+1, j);
      else
         return coreName;
   }

   public static String bestSysTitle(String displayName, String systemName)
   {
      if (!displayName.contains(" - "))
         return systemName;

      int i = displayName.indexOf('(');
      if (i > 0)
         return displayName.substring(0, i).trim();
      else
         return displayName;
   }
}