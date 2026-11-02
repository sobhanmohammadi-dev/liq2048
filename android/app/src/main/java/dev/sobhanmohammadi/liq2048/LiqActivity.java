package dev.sobhanmohammadi.liq2048;

import org.libsdl.app.SDLActivity;

/** The whole game runs in native code; SDL's activity hosts the surface, input and audio. */
public class LiqActivity extends SDLActivity {
    @Override
    protected String[] getLibraries() {
        return new String[] {"SDL2", "main"};
    }
}
