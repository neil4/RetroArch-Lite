package com.retroarch.browser.preferences.util;

import android.content.Context;
import android.content.SharedPreferences;
import android.media.AudioManager;
import android.os.Environment;
import android.preference.PreferenceManager;
import android.util.Log;

import java.io.File;
import java.io.IOException;

/**
 * Utility class for retrieving, saving, or loading preferences.
 */
public final class UserPreferences
{
   // Logging tag.
   private static final String TAG = "UserPreferences";
   public static final String defaultBaseDir = Environment.getExternalStorageDirectory().getAbsolutePath() + "/RetroArchLite";
   
   // Disallow explicit instantiation.
   private UserPreferences()
   {
   }

   /**
    * Retrieves the path to the default location of the libretro config.
    *
    * @param ctx the current {@link Context}
    *
    * @return the path to the default location of the libretro config.
    */
   public static String getDefaultConfigPath(Context ctx)
   {
      return ctx.getApplicationInfo().dataDir + "/retroarch.cfg";
   }

   /**
    * Re-reads the configuration file into the {@link SharedPreferences}
    * instance that contains all of the settings for the front-end.
    *
    * @param ctx the current {@link Context}.
    */
   public static void readbackConfigFile(Context ctx)
   {
      String path = getDefaultConfigPath(ctx);
      ConfigFile config = new ConfigFile(path);

      Log.i(TAG, "Config readback from: " + path);

      SharedPreferences prefs = getPreferences(ctx);
      SharedPreferences.Editor edit = prefs.edit();

      // Audio Settings.
      readbackString(config, edit, "audio_latency");

      // Video Settings
      readbackString(config, edit, "video_refresh_rate");
      
      // Menu Settings
      readbackBool(config, edit, "mame_titles");

      edit.commit();
   }

   /**
    * Updates the libretro configuration file
    * with new values if settings have changed.
    *
    * @param ctx the current {@link Context}.
    */
   public static void updateConfigFile(Context ctx)
   {
      final String configPath = getDefaultConfigPath(ctx);  // main config
      ConfigFile config = new ConfigFile(configPath);
      Log.i(TAG, "Writing config to: " + configPath);

      final String defaultSave = UserPreferences.defaultBaseDir + "/save";
      final String defaultSys = UserPreferences.defaultBaseDir + "/system";
      final String defaultConfig = UserPreferences.defaultBaseDir + "/config";  // configs, core options, and remaps
      final String defaultState = UserPreferences.defaultBaseDir + "/state";

      final SharedPreferences prefs = getPreferences(ctx);

      // Default ROM directory
      config.setString("rgui_browser_directory", prefs.getString("rgui_browser_directory", ""));
      
      // Audio, Video
      //
      config.setInt("audio_out_rate", getOptimalSamplingRate(ctx));
      if (prefs.getBoolean("audio_latency_auto", true))
         config.setInt("audio_block_frames", getLowLatencyBufferSize(ctx));
      else
         config.setInt("audio_latency", Integer.parseInt(prefs.getString("audio_latency", "64")));
      config.setString("video_refresh_rate", prefs.getString("video_refresh_rate", ""));
      
      // Save, State, System, & Config paths
      //
      String saveDir = prefs.getBoolean("savefile_directory_enable", false) ?
                       prefs.getString("savefile_directory", defaultSave) : defaultSave;
      config.setString("savefile_directory", saveDir);
      new File(saveDir).mkdirs();
       
      String stateDir = prefs.getBoolean("savestate_directory_enable", false) ?
                        prefs.getString("savestate_directory", defaultState) : defaultState;
      config.setString("savestate_directory", stateDir);
      new File(stateDir).mkdirs();

      String sysDir = prefs.getBoolean("system_directory_enable", false) ?
                      prefs.getString("system_directory", defaultSys) : defaultSys;
      config.setString("system_directory", sysDir);
      new File(sysDir).mkdirs();
      
      String cfgDir = prefs.getBoolean("config_directory_enable", false) ?
                      prefs.getString("rgui_config_directory", defaultConfig) : defaultConfig;
      config.setString("rgui_config_directory", cfgDir);
      config.setString("input_remapping_directory", cfgDir);
      new File(cfgDir).mkdirs();
      
      // Menu
      config.setBoolean("mame_titles", prefs.getBoolean("mame_titles", false));

      try
      {
         config.write(configPath);
      }
      catch (IOException e)
      {
         Log.e(TAG, "Failed to save config file to: " + configPath);
      }
   }

   public static void readbackString(ConfigFile cfg, SharedPreferences.Editor edit, String key)
   {
      if (cfg.keyExists(key))
         edit.putString(key, cfg.getString(key));
      else
         edit.remove(key);
   }

   private static void readbackBool(ConfigFile cfg, SharedPreferences.Editor edit, String key)
   {
      if (cfg.keyExists(key))
         edit.putBoolean(key, cfg.getBoolean(key));
      else
         edit.remove(key);
   }

   private static void readbackDouble(ConfigFile cfg, SharedPreferences.Editor edit, String key)
   {
      if (cfg.keyExists(key))
         edit.putFloat(key, (float)cfg.getDouble(key));
      else
         edit.remove(key);
   }

   private static void readbackFloat(ConfigFile cfg, SharedPreferences.Editor edit, String key)
   {
      if (cfg.keyExists(key))
         edit.putFloat(key, cfg.getFloat(key));
      else
         edit.remove(key);
   }

   /*
   private static void readbackInt(ConfigFile cfg, SharedPreferences.Editor edit, String key)
   {
      if (cfg.keyExists(key))
         edit.putInt(key, cfg.getInt(key));
      else
         edit.remove(key);
   }
   */
   
   /**
    * Gets a {@link SharedPreferences} instance containing current settings.
    *
    * @param ctx the current {@link Context}.
    *
    * @return A SharedPreference instance containing current settings.
    */
   public static SharedPreferences getPreferences(Context ctx)
   {
      return PreferenceManager.getDefaultSharedPreferences(ctx);
   }

   /**
    * Gets the optimal sampling rate for low-latency audio playback.
    *
    * @param ctx the current {@link Context}.
    *
    * @return the optimal sampling rate for low-latency audio playback in Hz.
    */
   private static int getLowLatencyOptimalSamplingRate(Context ctx)
   {
      AudioManager manager = (AudioManager) ctx.getSystemService(Context.AUDIO_SERVICE);

      return Integer.parseInt(manager
            .getProperty(AudioManager.PROPERTY_OUTPUT_SAMPLE_RATE));
   }

   /**
    * Gets the optimal buffer size for low-latency audio playback.
    *
    * @param ctx the current {@link Context}.
    *
    * @return the optimal output buffer size in decimal PCM frames.
    */
   private static int getLowLatencyBufferSize(Context ctx)
   {
      AudioManager manager = (AudioManager) ctx.getSystemService(Context.AUDIO_SERVICE);
      int buffersize = Integer.parseInt(manager
            .getProperty(AudioManager.PROPERTY_OUTPUT_FRAMES_PER_BUFFER));
      Log.i(TAG, "Queried ideal buffer size (frames): " + buffersize);
      return buffersize;
   }

   /**
    * Gets the optimal audio sampling rate.
    * This will retrieve the optimal low-latency sampling rate,
    * since Android 4.2 adds support for low latency audio in general.
    *
    * @param ctx The current {@link Context}.
    *
    * @return the optimal audio sampling rate in Hz.
    */
   private static int getOptimalSamplingRate(Context ctx)
   {
      int ret = getLowLatencyOptimalSamplingRate(ctx);

      Log.i(TAG, "Using sampling rate: " + ret + " Hz");
      return ret;
   }
}
