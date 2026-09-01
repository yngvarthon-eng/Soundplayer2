#include "SpectrogramPlugin.h"
#include <cmath>

SpectrogramPlugin::SpectrogramPlugin()
{
}

// Maps 0..1 magnitude to a hue cycle: black -> blue -> cyan -> green -> yellow -> red -> white
juce::Colour SpectrogramPlugin::spectrumToColour(float magnitude)
{
    magnitude = juce::jlimit(0.0f, 1.0f, magnitude);
    if (magnitude < 1e-4f)
        return juce::Colours::black;

    // Map magnitude to hue: 0 = blue (240°), 1 = red (0°), clamped
    float hue = (1.0f - magnitude) * 0.666f; // 0.666 ≈ blue hue in JUCE (0..1 range)
    float sat = 1.0f;
    float bri  = juce::jlimit(0.3f, 1.0f, magnitude * 2.0f + 0.2f);
    return juce::Colour::fromHSV(hue, sat, bri, 1.0f);
}

void SpectrogramPlugin::update(const juce::AudioBuffer<float>& /*buffer*/,
                               const juce::Array<float>& spectrum)
{
    latestSpectrum = spectrum;
}

void SpectrogramPlugin::render(juce::Graphics& g, juce::Rectangle<int> bounds)
{
    g.fillAll(juce::Colours::black);

    const int w = bounds.getWidth();
    const int h = bounds.getHeight();
    if (w <= 0 || h <= 0 || latestSpectrum.size() == 0)
        return;

    // (Re)create image if size changed
    if (!imageReady || image.getWidth() != w || image.getHeight() != h)
    {
        image = juce::Image(juce::Image::RGB, w, h, true);
        imageWritePos = 0;
        imageReady = true;
    }

    // Write a new column at imageWritePos
    {
        juce::Image::BitmapData bmp(image, juce::Image::BitmapData::readWrite);
        int numBins = latestSpectrum.size();
        for (int y = 0; y < h; ++y)
        {
            // y=0 is top (high freq), y=h-1 is bottom (low freq)
            float binF = (float)(h - 1 - y) / (h - 1) * (numBins - 1);
            int   bin0 = (int)binF;
            float frac = binF - bin0;
            int   bin1 = juce::jmin(bin0 + 1, numBins - 1);

            float mag = latestSpectrum[bin0] * (1.0f - frac) + latestSpectrum[bin1] * frac;
            // Apply mild log scaling so low-energy content is visible
            mag = std::sqrt(mag);
            bmp.setPixelColour(imageWritePos, y, spectrumToColour(mag));
        }
    }

    // Draw the image split around the write position (circular buffer layout)
    // Right part: columns from 0..imageWritePos drawn at the right side of the component
    // Left part:  columns from imageWritePos..w drawn at the left side
    int rightPartW = imageWritePos;
    int leftPartW  = w - imageWritePos;

    if (leftPartW > 0)
        g.drawImage(image,
                    bounds.getX(), bounds.getY(), leftPartW, h,
                    imageWritePos, 0, leftPartW, h);
    if (rightPartW > 0)
        g.drawImage(image,
                    bounds.getX() + leftPartW, bounds.getY(), rightPartW, h,
                    0, 0, rightPartW, h);

    imageWritePos = (imageWritePos + 1) % w;
}
