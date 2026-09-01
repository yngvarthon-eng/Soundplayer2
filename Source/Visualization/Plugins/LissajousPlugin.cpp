#include "LissajousPlugin.h"

LissajousPlugin::LissajousPlugin()
{
    samplesL.resize(kMaxPoints);
    samplesR.resize(kMaxPoints);
}

void LissajousPlugin::update(const juce::AudioBuffer<float>& buffer,
                             const juce::Array<float>& /*spectrum*/)
{
    int n = buffer.getNumSamples();
    if (n == 0) return;

    auto* L = buffer.getReadPointer(0);
    auto* R = (buffer.getNumChannels() > 1) ? buffer.getReadPointer(1) : L;

    // Stride to take at most kMaxPoints evenly-spaced samples
    int stride = juce::jmax(1, n / kMaxPoints);
    numPoints = 0;
    for (int i = 0; i < n && numPoints < kMaxPoints; i += stride)
    {
        samplesL[static_cast<size_t>(numPoints)] = L[i];
        samplesR[static_cast<size_t>(numPoints)] = R[i];
        ++numPoints;
    }
}

void LissajousPlugin::render(juce::Graphics& g, juce::Rectangle<int> bounds)
{
    const int w = bounds.getWidth();
    const int h = bounds.getHeight();
    g.fillAll(juce::Colours::black);

    if (w <= 0 || h <= 0 || numPoints == 0)
        return;

    // Fade the trail image into the background each frame
    if (!trailImage.isValid() || trailImage.getWidth() != w || trailImage.getHeight() != h)
        trailImage = juce::Image(juce::Image::ARGB, w, h, true);

    // Dim existing trail
    {
        juce::Graphics ig(trailImage);
        ig.setColour(juce::Colours::black.withAlpha(0.25f));
        ig.fillAll();
    }

    // Draw new points onto the trail
    {
        juce::Graphics ig(trailImage);
        const float cx = w * 0.5f;
        const float cy = h * 0.5f;
        const float scale = juce::jmin(cx, cy) * 0.9f;

        // Classic 45° rotation for correlation display:
        // plot_x = (L + R) / √2,  plot_y = (L - R) / √2
        static const float kSqrt2Inv = 0.7071068f;

        ig.setColour(juce::Colours::cyan.withAlpha(0.8f));
        for (int i = 0; i < numPoints; ++i)
        {
            float l = samplesL[static_cast<size_t>(i)];
            float r = samplesR[static_cast<size_t>(i)];
            float px = cx + (l + r) * kSqrt2Inv * scale;
            float py = cy - (l - r) * kSqrt2Inv * scale; // Y inverted in screen coords
            ig.fillEllipse(px - 1.0f, py - 1.0f, 2.0f, 2.0f);
        }
    }

    // Composite trail image onto the component
    g.drawImageAt(trailImage, bounds.getX(), bounds.getY());

    // Reference lines
    g.setColour(juce::Colours::darkgrey);
    g.drawVerticalLine(bounds.getCentreX(), (float)bounds.getY(), (float)bounds.getBottom());
    g.drawHorizontalLine(bounds.getCentreY(), (float)bounds.getX(), (float)bounds.getRight());

    // Labels
    g.setColour(juce::Colours::grey);
    g.setFont(9.0f);
    g.drawText("L", bounds.getX() + 4, bounds.getCentreY() - 16, 10, 10,
               juce::Justification::left);
    g.drawText("R", bounds.getRight() - 14, bounds.getCentreY() - 16, 10, 10,
               juce::Justification::left);
    g.drawText("M", bounds.getCentreX() - 5, bounds.getY() + 4, 10, 10,
               juce::Justification::centred);
    g.drawText("S", bounds.getCentreX() - 5, bounds.getBottom() - 14, 10, 10,
               juce::Justification::centred);
}
