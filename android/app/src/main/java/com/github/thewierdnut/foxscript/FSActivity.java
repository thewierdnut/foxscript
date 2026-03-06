package com.github.thewierdnut.foxscript;


import org.libsdl.app.SDLActivity;


import android.Manifest;
import android.content.Intent;
// import androidx.core.content.FileProvider;
import android.content.pm.PackageManager;
import android.os.Bundle;
import android.widget.Toast;
import android.app.Activity;
import android.graphics.Bitmap;
import android.graphics.BitmapFactory;
import android.provider.MediaStore;
import android.net.Uri;
import java.io.File;

import android.util.Log;



/**
    SDL Activity.
*/
public class FSActivity extends SDLActivity
{
    static int REQUEST_CAMERA_PERMISSION = 1;
    static int REQUEST_GALLERY_IMAGE = 2;

    protected String[] getLibraries()
    {
        return new String[] {
            // Everything is statically linked
            // "SDL2",
            // "SDL2_image",
            // "SDL2_mixer",
            // "SDL2_net",
            // "SDL2_ttf",
            "main"
        };
    }

    @Override
    protected void onCreate(Bundle savedInstanceState)
    {
        super.onCreate(savedInstanceState);
    }

    @Override
    public void onActivityResult(int requestCode, int resultCode, Intent data)
    {
        Log.e("SDL-Debug", "FSActivity::onActivityResult()");
        // if (requestCode == REQUEST_CAMERA_IMAGE)
        // {
        //     if (resultCode == Activity.RESULT_OK)
        //     {
        //         // Bundle extras = data.getExtras();
        //         // Bitmap imageBitmap = (Bitmap) extras.get("data");
        //         // Use the bitmap, for example, set it to an ImageView
        //         Toast.makeText(this, "Successfully retrieved a picture ", Toast.LENGTH_SHORT).show();

        //         Bitmap bitmap = BitmapFactory.decodeFile(getCacheDir() + "/tmp.jpg");
        //         Log.e("SDL-Debug", "Retrieved " + String.valueOf(bitmap.getWidth()) + "x" + String.valueOf(bitmap.getWidth()));

        //     }
        //     else
        //     {
        //         Toast.makeText(this, "Failed to take picture ", Toast.LENGTH_SHORT).show();
        //     }
        // }
    }

    @Override
    public void onRequestPermissionsResult(int requestCode, String[] permissions, int[] results)
    {
        if (requestCode == REQUEST_CAMERA_PERMISSION)
        {
            if (results[0] == PackageManager.PERMISSION_GRANTED )
            {
                Log.e("SDL-Debug", "Got permission to access camera.");
                CameraReady();
            }
            else
            {
                Log.e("SDL-Debug", "Permission to access camera was denied.");
            }
        }
    }

    public boolean CheckCameraPermissions()
    {
        if (checkSelfPermission(Manifest.permission.CAMERA) != PackageManager.PERMISSION_GRANTED)
        {
            requestPermissions(new String[]{Manifest.permission.CAMERA}, REQUEST_CAMERA_PERMISSION);
            return false;
        }
        return true;
    }

    public native void CameraReady();

    public void GetNewCameraImage()
    {
        // Log.e("SDL-Debug", "FSActivity::GetNewCameraImage()");

        // Intent takePictureIntent = new Intent(MediaStore.ACTION_IMAGE_CAPTURE);
        // if (takePictureIntent.resolveActivity(getPackageManager()) != null)
        // {
        //     File outputFile;
        //     try
        //     {
        //         outputFile = File.createTempFile("tmp.jpg", null, getCacheDir());
        //     }
        //     catch (java.io.IOException e)
        //     {
        //         Log.e("SDL-Debug", "FSActivity::GetNewCameraImage() -> caught exception trying to create temporary cache file");
        //         return;
        //     }

        //     if (outputFile != null)
        //     {
        //         Uri uri = FileProvider.getUriForFile(this, "com.example.android.fileprovider", outputFile);
        //         takePictureIntent.putExtra(MediaStore.EXTRA_OUTPUT, uri);
        //         Log.e("SDL-Debug", "FSActivity::GetNewCameraImage() -> startActivityForResult()");
        //         startActivityForResult(takePictureIntent, REQUEST_CAMERA_IMAGE);
        //     }
        //     else
        //     {
        //         Log.e("SDL-Debug", "FSActivity::GetNewCameraImage() Unable to create temporary image storage location");
        //     }

            
        // }
        // else
        // {
        //     Log.e("SDL-Debug", "FSActivity::GetNewCameraImage() resolveActivity returned false");
        // }
    }
}