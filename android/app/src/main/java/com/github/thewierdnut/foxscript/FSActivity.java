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
import java.io.InputStream;

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
        if (requestCode == REQUEST_GALLERY_IMAGE)
        {
            if (resultCode == Activity.RESULT_OK)
            {
                Uri imageUri = data.getData();
                try
                {
                    InputStream inputStream = getContentResolver().openInputStream(imageUri);
                    if (inputStream != null)
                    {
                        Bitmap bitmap = BitmapFactory.decodeStream(inputStream);
                        Log.e("SDL-Debug", "Retrieved " + String.valueOf(bitmap.getWidth()) + "x" + String.valueOf(bitmap.getWidth()));
                        ImageReady(bitmap);
                    }
                }
                catch (java.io.FileNotFoundException e)
                {
                    Log.e("SDL-Debug", "Caught FileNotFoundException trying to open " + imageUri.toString());
                }
            }
        }
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

    public void GetImage()
    {
        Log.e("SDL-Debug", "FSActivity::GetImage()");

        Intent get_image = new Intent(Intent.ACTION_PICK);
        get_image.setType("image/*");
        startActivityForResult(get_image, REQUEST_GALLERY_IMAGE);
    }

    public native void ImageReady(Bitmap b);
}