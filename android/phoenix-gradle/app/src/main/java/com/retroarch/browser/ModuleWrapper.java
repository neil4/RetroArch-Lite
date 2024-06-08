package com.retroarch.browser;

import android.content.Context;
import android.content.SharedPreferences;
import android.graphics.drawable.Drawable;
import android.os.Environment;

import com.retroarch.browser.preferences.util.ConfigFile;
import com.retroarch.browser.preferences.util.UserPreferences;
import com.retroarchlite.R;

import java.io.File;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.List;

/**
 * Wrapper class that encapsulates a libretro core 
 * along with information about said core.
 */
public final class ModuleWrapper implements IconAdapterItem, Comparable<ModuleWrapper>
{
   private final File file;
   private final String displayName;
   private final String coreName;
   private final List<String> manufacturer;
   private final String systemName;
   private final List<String> license;
   private final String notes;
   private final List<String> authors;
   private final List<String> supportedExtensions;
   private final List<String> requiredHwApi;
   private final String description;
   private int firmwareCount = 0;
   private final String firmwares;
   private final String coreTitle;
   private final String systemTitle;
   private final String titleText;
   private final String subText;

   /**
    * Constructor
    * 
    * @param context The current {@link Context}.
    * @param file    The {@link File} instance of the core being wrapped.
    * @param showAbi Whether to append (32b) or (64b) to core titles
    * @param sortBySys Whether to use systemTitle or coreTitle as titleText
    */
   public ModuleWrapper(Context context, File file, boolean showAbi, boolean sortBySys)
   {
      this.file = file;
      SharedPreferences prefs = UserPreferences.getPreferences(context);

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
         this.notes        = (infoFile.keyExists("notes"))        ? infoFile.getString("notes").replace("|", "\n")
                             : null;
         this.description  = (infoFile.keyExists("description"))  ? infoFile.getString("description") : "N/A";
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

         final String requiredHwApi = infoFile.getString("required_hw_api");
         if (requiredHwApi != null && requiredHwApi.contains("|"))
         {
            this.requiredHwApi = new ArrayList<String>(Arrays.asList(requiredHwApi.split("\\|")));
         }
         else
         {
            this.requiredHwApi = null;
         }
         
         // Firmware list
         //
         if (this.firmwareCount == 0)
         {
            this.firmwares = "N/A";
         }
         else
         {
            final String defaultBase = Environment.getExternalStorageDirectory()
                                                  .getAbsolutePath() + "/RetroArchLite";
            final String defaultSys = defaultBase + "/system";
            String sysDir = prefs.getBoolean("system_directory_enable", false) ?
                            prefs.getString("system_directory", defaultSys) : defaultSys;
            StringBuilder sb = new StringBuilder();
            for (int i = 0; i < this.firmwareCount; i++)
            {
               String key = "firmware" + i + "_desc";
               sb.append((infoFile.keyExists(key)) ? infoFile.getString(key) : "N/A");

               key = "firmware" + i + "_path";
               String relPath = ((infoFile.keyExists(key)) ? infoFile.getString(key) : "?");
               final boolean present = new File(sysDir + '/' + relPath).exists();

               key = "firmware" + i + "_opt";
               final boolean optional = infoFile.keyExists(key) && infoFile.getBoolean(key);

               sb.append( "\n\t")
                 .append(present  ? '+' : (optional ? '-' : '!'))
                 .append(present  ? " present" : " missing")
                 .append(optional ? ", optional" : ", required")
                 .append('\n');
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
         this.notes = null;
         this.authors = new ArrayList<String>();
         this.authors.add("Unknown");
         this.supportedExtensions = new ArrayList<String>();
         this.coreName = coreName;
         this.requiredHwApi = null;
         this.firmwares = "Unknown";
         this.description = "N/A";
      }

      // Set displayed title and subtext
      boolean is64bit = file.getAbsolutePath().contains("/com.retroarchlite64/");
      coreTitle = bestCoreTitle(this.displayName, this.coreName)
            + (showAbi ? (is64bit ? " (64b)" : " (32b)") : "");
      systemTitle  = bestSystemTitle(this.displayName, this.systemName);

      titleText = sortBySys ? systemTitle : coreTitle;
      subText   = sortBySys ? coreTitle   : systemTitle;
   }

   /**
    * Same as the original constructor, but allows for string paths.
    * 
    * @param context The current {@link Context}.
    * @param path    Path to the file to encapsulate.
    * @param showAbi Whether to append (32b) or (64b) to core titles
    * @param sortBySys Whether to use systemTitle or coreTitle as titleText
    */
   public ModuleWrapper(Context context, String path, boolean showAbi, boolean sortBySys)
   {
      this(context, new File(path), showAbi, sortBySys);
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
    * Gets the description for this wrapped core.
    *
    * @return the description for this wrapped core.
    */
   public String getDescription()
   {
      return description;
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
    * Gets the core title to display for this wrapped core.
    *
    * @return the core title for this wrapped core.
    */
   public String getCoreTitle()
   {
      return coreTitle;
   }

   /**
    * Gets the system title to display for this wrapped core.
    *
    * @return the system title for this wrapped core.
    */
   public String getSystemTitle()
   {
      return systemTitle;
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
    * Gets the List of supported extensions for this core.
    * 
    * @return the List of supported extensions for this core.
    */
   public List<String> getSupportedExtensions()
   {
      return supportedExtensions;
   }

   @Override
   public boolean isEnabled()
   {
      return true;
   }

   @Override
   public String getText()
   {
      return titleText;
   }
   
   @Override
   public String getSubText()
   {
      return subText;
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
      return titleText.compareToIgnoreCase(other.titleText);
   }

   public static String bestCoreTitle(String displayName, String coreName)
   {
      int i = displayName.indexOf(" (") + 2;
      int j = displayName.lastIndexOf(')');
      if (i < 2 || coreName == null)
         return displayName;
      if (i > 2 && j > i && j-i > coreName.length())
         return displayName.substring(i, j);
      return coreName;
   }

   public static String bestSystemTitle(String displayName, String systemName)
   {
      int i = displayName.indexOf(" (");
      if (i > 1)
         return displayName.substring(0, i);
      if (systemName != null && !systemName.isEmpty())
         return systemName;
      return displayName;
   }
}