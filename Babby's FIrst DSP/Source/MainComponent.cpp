#include "MainComponent.h"

//==============================================================================
MainComponent::MainComponent():state (Stopped)
{
    addAndMakeVisible (&openButton);
    openButton.setButtonText ("Open...");
    openButton.onClick = [this] { openButtonClicked(); };
    
    addAndMakeVisible (&playButton);
    playButton.setButtonText ("Play");
    playButton.onClick = [this] { playButtonClicked(); };
    playButton.setColour (juce::TextButton::buttonColourId, juce::Colours::green);
    playButton.setEnabled (false);
    
    addAndMakeVisible (&stopButton);
    stopButton.setButtonText ("Stop");
    stopButton.onClick = [this] { stopButtonClicked(); };
    stopButton.setColour (juce::TextButton::buttonColourId, juce::Colours::red);
    stopButton.setEnabled (false);
    
    addAndMakeVisible (&progressLabel);
    progressLabel.setText("0 s", juce::sendNotification);
    progressLabel.setColour(juce::Label::textColourId, juce::Colours::green);
    
    addAndMakeVisible(&roomSize);
    roomSize.setTitle("Room Size:");
    roomSize.setRange(0.0, 1.0);
    roomSize.setValue(reverbParameters.roomSize);
    roomSize.onValueChange = [this] { reverbParameterChanged(); };
    
    addAndMakeVisible(&wetMix);
    wetMix.setTitle("Wet:");
    wetMix.setRange(0.0, 1.0);
    wetMix.setValue(reverbParameters.wetLevel);
    wetMix.onValueChange = [this] { reverbParameterChanged(); };
    
    addAndMakeVisible(&dryMix);
    dryMix.setTitle("Dry:");
    dryMix.setRange(0.0, 1.0);
    dryMix.setValue(reverbParameters.dryLevel);
    dryMix.onValueChange = [this] { reverbParameterChanged(); };
    
    formatManager.registerBasicFormats();       // [1]

    transportSource.addChangeListener (this);   // [2]

    // Make sure you set the size of the component after
    // you add any child components.
    setSize (300, 200);

    // Some platforms require permissions to open input channels so request that here
    if (juce::RuntimePermissions::isRequired (juce::RuntimePermissions::recordAudio)
        && ! juce::RuntimePermissions::isGranted (juce::RuntimePermissions::recordAudio))
    {
        juce::RuntimePermissions::request (juce::RuntimePermissions::recordAudio,
                                           [&] (bool granted) { setAudioChannels (granted ? 2 : 0, 2); });
    }
    else
    {
        // Specify the number of input and output channels that we want to open
        setAudioChannels (2, 2);
    }
    
    startTimer(20);
}

MainComponent::~MainComponent()
{
    // This shuts down the audio device and clears the audio source.
    shutdownAudio();
}

//==============================================================================
void MainComponent::prepareToPlay (int samplesPerBlockExpected, double sampleRate)
{
    expectedSamplesPerBlock = samplesPerBlockExpected;
    expectedSampleRate = sampleRate;
}

void MainComponent::getNextAudioBlock (const juce::AudioSourceChannelInfo& bufferToFill)
{
    if (readerSource.get() == nullptr)
    {
        bufferToFill.clearActiveBufferRegion();
        return;
    }

    readerSource->getNextAudioBlock(bufferToFill);
    auto* leftRead = bufferToFill.buffer->getWritePointer(0, bufferToFill.startSample);
    auto* rightRead = bufferToFill.buffer->getWritePointer(1, bufferToFill.startSample);

    reverb.processStereo(leftRead, rightRead, bufferToFill.numSamples);
}

void MainComponent::releaseResources()
{
    // This will be called when the audio device stops, or when it is being
    // restarted due to a setting change.

    // For more details, see the help for AudioProcessor::releaseResources()
    
    transportSource.releaseResources();
    readerSource->releaseResources();
}

//==============================================================================
void MainComponent::paint (juce::Graphics& g)
{
    // (Our component is opaque, so we must completely fill the background with a solid colour)
    g.fillAll (getLookAndFeel().findColour (juce::ResizableWindow::backgroundColourId));

    // You can add your drawing code here!
}

void MainComponent::resized()
{
    openButton.setBounds (10, 10, getWidth() - 20, 20);
    playButton.setBounds (10, 40, getWidth() - 20, 20);
    stopButton.setBounds (10, 70, getWidth() - 20, 20);
    roomSize.setBounds (10, 100, getWidth() - 20, 20);
    wetMix.setBounds (10, 130, getWidth() - 20, 20);
    dryMix.setBounds (10, 160, getWidth() - 20, 20);
    progressLabel.setBounds(10, 190, getWidth() - 20, 20);
}

void MainComponent::changeState (MainComponent::TransportState newState)
{
    if (state != newState)
    {
        state = newState;
        
        switch (state)
        {
            case MainComponent::Stopped:                           // [3]
                playButton.setButtonText("Play");
                stopButton.setButtonText("Stop");
                stopButton.setEnabled (false);
                readerSource->setNextReadPosition(0);
                break;
                
            case MainComponent::Starting:                          // [4]
                readerSource.reset (newSource.release());
                readerSource->prepareToPlay(expectedSamplesPerBlock, expectedSampleRate);
                break;
                
            case MainComponent::Playing:                           // [5]
                playButton.setButtonText("Pause");
                stopButton.setButtonText("Stop");
                stopButton.setEnabled (true);
                break;
                
            case MainComponent::Pausing:
                transportSource.stop();
                break;
            case MainComponent::Paused:
                playButton.setButtonText("Resume");
                stopButton.setButtonText("Return to 0");
                break;
            case MainComponent::Stopping:                          // [6]
                transportSource.stop();
                break;
        }
    }
}

void MainComponent::changeListenerCallback (juce::ChangeBroadcaster* source)
{
    if (source == &transportSource) {
        if (transportSource.isPlaying()) {
            changeState(Playing);
        } else if ((state == Stopping) || (state == Playing)) {
            changeState(Stopped);
        } else if (state == Pausing) {
            changeState(Paused);
        }
    }
}

void MainComponent::openButtonClicked()
{
    chooser = std::make_unique<juce::FileChooser> ("Select wav file to play...",
                                                   juce::File{},
                                                   "*.wav");
    auto chooserFlags = juce::FileBrowserComponent::openMode
    | juce::FileBrowserComponent::canSelectFiles;
    
    chooser->launchAsync (chooserFlags, [this] (const juce::FileChooser& fc)
                          {
        auto file = fc.getResult();
        if (file != juce::File{}) {
            auto *reader = formatManager.createReaderFor (file);
            if (reader != nullptr) {
                newSource = std::make_unique<juce::AudioFormatReaderSource> (reader, true);
                playButton.setEnabled(true);
            }
        }
    });
}
                          
void MainComponent::playButtonClicked()
{
    if ((state == Stopped) || (state == Paused)) {
        changeState(Starting);
    } else if (state == Playing) {
        changeState(Pausing);
    }
}

void MainComponent::stopButtonClicked()
{
    if (state == Paused) {
        changeState(Stopped);
    } else {
        changeState(Stopping);
    }
}

void MainComponent::timerCallback()
{
    if (transportSource.isPlaying()) {
        juce::RelativeTime position (transportSource.getCurrentPosition());
        auto minutes = ((int) position.inMinutes()) % 60;
        auto seconds = ((int) position.inSeconds()) % 60;
        auto millis = ((int) position.inMilliseconds()) % 1000;
        
        auto positionString = juce::String::formatted("%02d:%02d.%03d", minutes, seconds, millis);
        progressLabel.setText(positionString, juce::dontSendNotification);
    } else {
        progressLabel.setText("Stopped", juce::dontSendNotification);
    }
    
}

void MainComponent::reverbParameterChanged() {
    reverbParameters.roomSize = roomSize.getValue();
    reverbParameters.wetLevel = wetMix.getValue();
    reverbParameters.dryLevel = dryMix.getValue();
    
    reverb.setParameters(reverbParameters);
}
