package com.retroarch.browser.preferences.util;

import java.io.BufferedReader;
import java.io.FileInputStream;
import java.io.IOException;
import java.io.InputStreamReader;

import android.annotation.TargetApi;
import android.content.Context;
import android.content.SharedPreferences;
import android.media.AudioManager;
import android.media.AudioTrack;
import android.os.Build;
import android.preference.PreferenceManager;
import android.util.Log;
import android.os.Environment;
import java.io.File;

/**
 * Utility class for retrieving, saving, or loading preferences.
 */
public final class UserPreferences
{
	// Logging tag.
	private static final String TAG = "UserPreferences";
   
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

		// Input Settings
		readbackString(config, edit, "input_overlay");
		readbackBool(config, edit, "input_overlay_enable");
		readbackBool(config, edit, "input_autodetect_enable");

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
      final String config_path = getDefaultConfigPath(ctx);  // main config
      ConfigFile config = new ConfigFile(config_path);
      Log.i(TAG, "Writing config to: " + config_path);

      final String default_base = Environment.getExternalStorageDirectory().getAbsolutePath() + "/RetroArchLite";
      final String default_save = default_base + "/save";
      final String default_sys = default_base + "/system";
      final String default_config = default_base + "/config";  // content configs, core options, and remaps
      final String default_state = default_base + "/state";
      final String dataDir = ctx.getApplicationInfo().dataDir;
      final String coreDir = dataDir + "/cores/";

      final SharedPreferences prefs = getPreferences(ctx);
		
      // Internal directories
      //
      config.setString("libretro_directory", coreDir);
      config.setString("rgui_browser_directory", prefs.getString("rgui_browser_directory", ""));
      
      // Audio, Video
      //
		config.setInt("audio_out_rate", getOptimalSamplingRate(ctx));
      if (Build.VERSION.SDK_INT >= 17 && prefs.getBoolean("audio_latency_auto", true))
         config.setInt("audio_block_frames", getLowLatencyBufferSize(ctx));
      else
         config.setInt("audio_latency", Integer.parseInt(prefs.getString("audio_latency", "64")));
      config.setString("video_refresh_rate", prefs.getString("video_refresh_rate", ""));
      
      // Save, State, System, & Config paths
      //
      String save_dir = prefs.getBoolean("savefile_directory_enable", false) ?
                        prefs.getString("savefile_directory", default_save) : default_save;
      config.setString("savefile_directory", save_dir);
      new File(save_dir).mkdirs();
       
      String state_dir = prefs.getBoolean("savestate_directory_enable", false) ?
                         prefs.getString("savestate_directory", default_state) : default_state;
      config.setString("savestate_directory", state_dir);
      new File(state_dir).mkdirs();

      String sys_dir = prefs.getBoolean("system_directory_enable", false) ?
                       prefs.getString("system_directory", default_sys) : default_sys;
      config.setString("system_directory", sys_dir);
      new File(sys_dir).mkdirs();
      
      String cfg_dir = prefs.getBoolean("config_directory_enable", false) ?
                       prefs.getString("rgui_config_directory", default_config) : default_config;
      config.setString("rgui_config_directory", cfg_dir);
      config.setString("input_remapping_directory", cfg_dir);
      new File(cfg_dir).mkdirs();
      
      // Input
      //
      config.setBoolean("input_overlay_enable", prefs.getBoolean("input_overlay_enable", true));
      config.setBoolean("input_autodetect_enable", prefs.getBoolean("input_autodetect_enable", true));
      
      // Menu
      config.setBoolean("mame_titles",
                        prefs.getBoolean("mame_titles", false));

      try
      {
         config.write(config_path);
      }
      catch (IOException e)
      {
         Log.e(TAG, "Failed to save config file to: " + config_path);
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
	@TargetApi(17)
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
	@TargetApi(17)
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
	 * <p>
	 * On Android 4.2+ devices this will retrieve the optimal low-latency sampling rate,
	 * since Android 4.2 adds support for low latency audio in general.
	 * <p>
	 * On other devices, it simply returns the regular optimal sampling rate
	 * as returned by the hardware.
	 * 
	 * @param ctx The current {@link Context}.
	 * 
	 * @return the optimal audio sampling rate in Hz.
	 */
	private static int getOptimalSamplingRate(Context ctx)
	{
		int ret;
		if (Build.VERSION.SDK_INT >= 17)
			ret = getLowLatencyOptimalSamplingRate(ctx);
		else
			ret = AudioTrack.getNativeOutputSampleRate(AudioManager.STREAM_MUSIC);

		Log.i(TAG, "Using sampling rate: " + ret + " Hz");
		return ret;
	}
}
