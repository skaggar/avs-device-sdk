/*
 * SpeechSynthesizer.h
 *
 * Copyright 2017 Amazon.com, Inc. or its affiliates. All Rights Reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License").
 * You may not use this file except in compliance with the License.
 * A copy of the License is located at
 *
 *     http://aws.amazon.com/apache2.0/
 *
 * or in the "license" file accompanying this file. This file is distributed
 * on an "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either
 * express or implied. See the License for the specific language governing
 * permissions and limitations under the License.
 */

#ifndef ALEXA_CLIENT_SDK_CAPABILITY_AGENTS_SPEECH_SYNTHESIZER_INCLUDE_SPEECH_SYNTHESIZER_SPEECH_SYNTHESIZER_H_
#define ALEXA_CLIENT_SDK_CAPABILITY_AGENTS_SPEECH_SYNTHESIZER_INCLUDE_SPEECH_SYNTHESIZER_SPEECH_SYNTHESIZER_H_

#include <memory>
#include <mutex>
#include <string>
#include <unordered_set>
#include <deque>

#include <AVSCommon/AVS/AVSDirective.h>
#include <AVSCommon/AVS/CapabilityAgent.h>
#include <AVSCommon/AVS/Attachment/AttachmentManagerInterface.h>
#include <AVSCommon/SDKInterfaces/SpeechSynthesizerObserver.h>
#include <AVSCommon/SDKInterfaces/ExceptionEncounteredSenderInterface.h>
#include <AVSCommon/SDKInterfaces/ContextManagerInterface.h>
#include <AVSCommon/SDKInterfaces/DirectiveSequencerInterface.h>
#include <AVSCommon/SDKInterfaces/FocusManagerInterface.h>
#include <AVSCommon/SDKInterfaces/MessageSenderInterface.h>
#include <AVSCommon/Utils/MediaPlayer/MediaPlayerInterface.h>
#include <AVSCommon/Utils/MediaPlayer/MediaPlayerObserverInterface.h>
#include <AVSCommon/Utils/RequiresShutdown.h>
#include <AVSCommon/Utils/Threading/Executor.h>

namespace alexaClientSDK {
namespace capabilityAgents {
namespace speechSynthesizer {

/**
 * This class implements the SpeechSynthesizer capability agent.
 * @see https://developer.amazon.com/public/solutions/alexa/alexa-voice-service/reference/speechsynthesizer
 */
class SpeechSynthesizer
        : public avsCommon::avs::CapabilityAgent
        , public avsCommon::utils::mediaPlayer::MediaPlayerObserverInterface
        , public avsCommon::utils::RequiresShutdown
        , public std::enable_shared_from_this<SpeechSynthesizer> {
public:
    /// Alias to the @c SpeechSynthesizerObserver for brevity.
    using SpeechSynthesizerObserver = avsCommon::sdkInterfaces::SpeechSynthesizerObserver;

    /**
     * Create a new @c SpeechSynthesizer instance.
     *
     * @param mediaPlayer The instance of the @c MediaPlayerInterface used to play audio.
     * @param messageSender The instance of the @c MessageSenderInterface used to send events to AVS.
     * @param focusManager The instance of the @c FocusManagerInterface used to acquire focus of a channel.
     * @param contextManager The instance of the @c ContextObserverInterface to use to set the context
     * of the @c SpeechSynthesizer.
     * @param attachmentManager The instance of the @c AttachmentManagerInterface to use to read the attachment.
     * @param exceptionSender The instance of the @c ExceptionEncounteredSenderInterface to use to notify AVS
     * when a directive cannot be processed.
     *
     * @return Returns a new @c SpeechSynthesizer, or @c nullptr if the operation failed.
     */
    static std::shared_ptr<SpeechSynthesizer> create(
        std::shared_ptr<avsCommon::utils::mediaPlayer::MediaPlayerInterface> mediaPlayer,
        std::shared_ptr<avsCommon::sdkInterfaces::MessageSenderInterface> messageSender,
        std::shared_ptr<avsCommon::sdkInterfaces::FocusManagerInterface> focusManager,
        std::shared_ptr<avsCommon::sdkInterfaces::ContextManagerInterface> contextManager,
        std::shared_ptr<avsCommon::avs::attachment::AttachmentManagerInterface> attachmentManager,
        std::shared_ptr<avsCommon::sdkInterfaces::ExceptionEncounteredSenderInterface> exceptionSender);

    avsCommon::avs::DirectiveHandlerConfiguration getConfiguration() const override;

    /**
     * Add an observer to the SpeechSynthesizer.
     *
     * @param observer The observer to add.
     */
    void addObserver(std::shared_ptr<SpeechSynthesizerObserver> observer);

    /**
     * Remove an observer from the SpeechSynthesizer.
     *
     * @note This is a synchronous call which can not be made by an observer callback.  Attempting to call
     *     @c removeObserver() from @c SpeechSynthesizerObserver::onStateChanged() will result in a deadlock.
     *
     * @param observer The observer to remove.
     */
    void removeObserver(std::shared_ptr<SpeechSynthesizerObserver> observer);

    void onDeregistered() override;

    void handleDirectiveImmediately(std::shared_ptr<avsCommon::avs::AVSDirective> directive) override;

    void preHandleDirective(std::shared_ptr<DirectiveInfo> info) override;

    void handleDirective(std::shared_ptr<DirectiveInfo> info) override;

    void cancelDirective(std::shared_ptr<DirectiveInfo> info) override;

    void onFocusChanged(avsCommon::avs::FocusState newFocus) override;

    void provideState(const unsigned int stateRequestToken) override;

    void onContextAvailable(const std::string& jsonContext) override;

    void onContextFailure(const avsCommon::sdkInterfaces::ContextRequestError error) override;

    void onPlaybackStarted() override;

    void onPlaybackFinished() override;

    void onPlaybackError(const avsCommon::utils::mediaPlayer::ErrorType& type, std::string error) override;

private:
    /**
     * This class has all the data that is needed to process @c Speak directives.
     */
    class SpeakDirectiveInfo {
    public:
        /**
         * Constructor.
         *
         * @param directiveInfo The @c DirectiveInfo.
         */
        SpeakDirectiveInfo(std::shared_ptr<DirectiveInfo> directiveInfo);

        /// Release Speak specific resources.
        void clear();

        /// @c AVSDirective that is passed during preHandle.
        std::shared_ptr<avsCommon::avs::AVSDirective> directive;

        /// @c DirectiveHandlerResultInterface.
        std::shared_ptr<avsCommon::sdkInterfaces::DirectiveHandlerResultInterface> result;

        /// The token for this Speak directive.
        std::string token;

        /// The @c AttachmentReader from which to read speech audio.
        std::unique_ptr<avsCommon::avs::attachment::AttachmentReader> attachmentReader;

        /// A flag to indicate if an event needs to be sent to AVS on playback finished.
        bool sendPlaybackFinishedMessage;

        /// A flag to indicate if the directive complete message has to be sent to the @c DirectiveSequencer.
        bool sendCompletedMessage;
    };

    /**
     * Constructor
     *
     * @param mediaPlayer The instance of the @c MediaPlayerInterface used to play audio.
     * @param messageSender The instance of the @c MessageSenderInterface used to send events to AVS.
     * @param focusManager The instance of the @c FocusManagerInterface used to acquire focus of a channel.
     * @param contextManager The instance of the @c ContextObserverInterface to use to set the context
     * of the SpeechSynthesizer.
     * @param attachmentManager The instance of the @c AttachmentManagerInterface to use to read the attachment.
     * @param exceptionSender The instance of the @c ExceptionEncounteredSenderInterface to use to notify AVS
     * when a directive cannot be processed.
     */
    SpeechSynthesizer(
        std::shared_ptr<avsCommon::utils::mediaPlayer::MediaPlayerInterface> mediaPlayer,
        std::shared_ptr<avsCommon::sdkInterfaces::MessageSenderInterface> messageSender,
        std::shared_ptr<avsCommon::sdkInterfaces::FocusManagerInterface> focusManager,
        std::shared_ptr<avsCommon::sdkInterfaces::ContextManagerInterface> contextManager,
        std::shared_ptr<avsCommon::avs::attachment::AttachmentManagerInterface> attachmentManager,
        std::shared_ptr<avsCommon::sdkInterfaces::ExceptionEncounteredSenderInterface> exceptionSender);

    void doShutdown() override;

    /**
     * Initializes the @c SpeechSynthesizer.
     * Adds the @c SpeechSynthesizer as an observer of the speech player.
     * Adds the @c SpeechSynthesizer as a state provider with the @c ContextManager.
     */
    void init();

    /**
     * Handle a SpeechSynthesizer.Speak directive (on the @c m_executor thread) immediately. Starts playing the speech
     * associated with a directive.
     *
     * @param info The directive to handle immediately. The result object is a @c nullptr.
     */
    void executeHandleImmediately(std::shared_ptr<DirectiveInfo> info);

    /**
     * Pre-handle a SpeechSynthesizer.Speak directive (on the @c m_executor thread). Starts any caching of
     * attachment data for playing the speech associated with the directive. This needs to be called after
     * @c DirectiveInfo, the @c AVSDirective and @c DirectiveHandlerResultInterface have been checked to be valid.
     * @c DirectiveHandlerResultInterface is not needed for @c executeHandleImmediately.
     *
     * @param speakInfo The directive to pre-handle and the associated data.
     */
    void executePreHandleAfterValidation(std::shared_ptr<SpeakDirectiveInfo> speakInfo);

    /**
     * Handle a SpeechSynthesizer.Speak directive (on the @c m_executor thread).  This starts a request for
     * the foreground focus. This needs to be called after @c DirectiveInfo, the @c AVSDirective and
     * @c DirectiveHandlerResultInterface have been checked to be valid. @c DirectiveHandlerResultInterface is not
     * needed for @c executeHandleImmediately.
     *
     * @param speakInfo The directive to handle and the associated data.
     */
    void executeHandleAfterValidation(std::shared_ptr<SpeakDirectiveInfo> speakInfo);

    /**
     * Pre-handle a SpeechSynthesizer.Speak directive (on the @c m_executor thread). Starts any caching of
     * attachment data for playing the speech associated with the directive.
     *
     * @param info The directive to preHandle and the result object with which to communicate the result.
     */
    void executePreHandle(std::shared_ptr<DirectiveInfo> info);

    /**
     * Handle a SpeechSynthesizer.Speak directive (on the @c m_executor thread).  This starts a request for
     * the foreground focus.
     *
     * @param info The directive to handle and the result object with which to communicate the result.
     */
    void executeHandle(std::shared_ptr<DirectiveInfo> info);

    /**
     * Cancel execution of a SpeechSynthesizer.Speak directive (on the @c m_executor thread).
     *
     * @param info The directive to cancel.
     */
    void executeCancel(std::shared_ptr<DirectiveInfo> info);

    /**
     * Execute a change of state (on the @c m_executor thread). If the @c m_desiredState is @c PLAYING, playing the
     * audio of the current directive is started. If the @c m_desiredState is @c FINISHED this method triggers
     * termination of playing the audio.
     */
    void executeStateChange();

    /**
     * Request to provide an update of the SpeechSynthesizer's state to the ContextManager (on the @c m_executor
     * thread).
     *
     * @param state The state of the @c SpeechSynthesizer.
     * @param stateRequestToken The token to pass through when setting the state.
     */
    void executeProvideState(
        const SpeechSynthesizerObserver::SpeechSynthesizerState& state,
        const unsigned int& stateRequestToken);

    /**
     * Handle (on the @c m_executor thread) notification that speech playback has started.
     */
    void executePlaybackStarted();

    /**
     * Handle (on the @c m_executor thread) notification that speech playback has finished.
     */
    void executePlaybackFinished();

    /**
     * Handle (on the @c m_executor thread) notification that speech playback encountered an error.
     *
     * @param error Text describing the nature of the error.
     */
    void executePlaybackError(const avsCommon::utils::mediaPlayer::ErrorType& type, std::string error);

    /**
     * Builds the JSON state to be updated in the @c ContextManager.
     *
     * @param token The text to speech token sent in the @c Speak directive.
     * @param offsetInMilliseconds The current offset of text to speech in milliseconds.
     * @return The JSON state string.
     */
    std::string buildState(std::string& token, int64_t offsetInMilliseconds) const;

    /**
     * Builds a JSON payload string part of the event to be sent to AVS.
     *
     * @param token the token sent in the message from AVS.
     * @return The JSON payload string.
     */
    static std::string buildPayload(std::string& token);

    /**
     * Start playing Speak directive audio.
     */
    void startPlaying();

    /**
     * Stop playing Speak directive audio.
     */
    void stopPlaying();

    /**
     * Set the current state of the @c SpeechSynthesizer. It updates the @c ContextManager with the new state and send
     * an event with the updated state to AVS.
     * @c m_mutex must be acquired before calling this function.
     *
     * @param newState The new state of the @c SpeechSynthesizer.
     */
    void setCurrentStateLocked(SpeechSynthesizerObserver::SpeechSynthesizerState newState);

    /**
     * Set the desired state the @c SpeechSynthesizer needs to transition to based on the @c newFocus.
     * @c m_mutex must be acquired before calling this function.
     *
     * @param newFocus The new focus of the @c SpeechSynthesizer.
     */
    void setDesiredStateLocked(avsCommon::avs::FocusState newFocus);

    /**
     * Reset @c m_currentInfo, cleaning up any @c SpeechSynthesizer resources and removing from CapabilityAgent's
     * map of active @c AVSDirectives.
     *
     * @param info The @c DirectiveInfo instance to make current (defaults to @c nullptr).
     */
    void resetCurrentInfo(std::shared_ptr<SpeakDirectiveInfo> info = nullptr);

    /**
     * Send the handling completed notification and clean up the resources of @c m_currentInfo.
     */
    void setHandlingCompleted();

    /**
     * Send the handling failed notification and clean up the resources of @c m_currentInfo.
     *
     * @param description
     */
    void setHandlingFailed(const std::string& description);

    /**
     * Send ExceptionEncountered and report a failure to handle the @c AVSDirective.
     *
     * @param info The @c AVSDirective that encountered the error and ancillary information.
     * @param type The type of Exception that was encountered.
     * @param message The error message to include in the ExceptionEncountered message.
     */
    void sendExceptionEncounteredAndReportFailed(
        std::shared_ptr<SpeakDirectiveInfo> info,
        avsCommon::avs::ExceptionErrorType type,
        const std::string& message);

    /**
     * Send ExceptionEncountered because a required property was not in the @c AVSDirective's payload.
     *
     * @param info The @c AVSDirective that that was missing the property.
     * @param missingProperty The name of the missing property.
     */
    void sendExceptionEncounteredAndReportMissingProperty(
        std::shared_ptr<SpeakDirectiveInfo> info,
        const std::string& missingProperty);

    /**
     * Release the @c FOREGROUND focus (if we have it).
     */
    void releaseForegroundFocus();

    /**
     * Verify a pointer to a well formed @c SpeakDirectiveInfo.
     *
     * @param caller Name of the method making the call, for logging.
     * @param info The @c DirectiveInfo to test.
     * @param checkResult Check if the @c DirectiveHandlerResultInterface is not a @c nullptr in the @c DirectiveInfo.
     * @return A @c SpeakDirectiveInfo if it is well formed, otherwise @c nullptr.
     */
    std::shared_ptr<SpeakDirectiveInfo> validateInfo(
        const std::string& caller,
        std::shared_ptr<DirectiveInfo> info,
        bool checkResult = true);

    /**
     * Find the @c SpeakDirectiveInfo instance (if any) for the specified @c messageId.
     *
     * @param messageId The messageId value to find @c SpeakDirectiveInfo for.
     * @return The @c SpeakDirectiveInfo instance for @c messageId.
     */
    std::shared_ptr<SpeakDirectiveInfo> getSpeakDirectiveInfo(const std::string& messageId);

    /**
     * Checks if the @c messageId is already present in the map. If its not present, adds an entry to the map.
     * @param messageId The @c messageId value to add to the @c m_speakDirectiveInfoMap
     * @param speakDirectiveInfo The @c SpeakDirectiveInfo corresponding to the @c messageId to add.
     * @return @c true if @c messageId to @c SpeakDirectiveInfo mapping was added. @c false if entry already exists
     * for the @c messageId.
     */
    bool setSpeakDirectiveInfo(
        const std::string& messageId,
        std::shared_ptr<SpeechSynthesizer::SpeakDirectiveInfo> speakDirectiveInfo);

    /**
     * Adds the @c SpeakDirectiveInfo to the @c m_speakInfoQueue. If the entry is being added to an empty queue,
     * it initiates @c executeHandle on the @c SpeakDirectiveInfo.
     * @param speakInfo The @c SpeakDirectiveInfo to add to the @c m_speakInfoQueue.
     */
    void addToDirectiveQueue(std::shared_ptr<SpeakDirectiveInfo> speakInfo);

    /**
     * Removes the @c SpeakDirectiveInfo corresponding to the @c messageId from the @c m_speakDirectiveInfoMap.
     *
     * @param messageId The @c messageId to remove.
     */
    void removeSpeakDirectiveInfo(const std::string& messageId);

    /// MediaPlayerInterface instance to send audio attachments to
    std::shared_ptr<avsCommon::utils::mediaPlayer::MediaPlayerInterface> m_speechPlayer;

    /// Object used to send events.
    std::shared_ptr<avsCommon::sdkInterfaces::MessageSenderInterface> m_messageSender;

    /// The @c FocusManager used to acquire the channel.
    std::shared_ptr<avsCommon::sdkInterfaces::FocusManagerInterface> m_focusManager;

    /// The @c ContextManager that needs to be updated of the state.
    std::shared_ptr<avsCommon::sdkInterfaces::ContextManagerInterface> m_contextManager;

    /// The @c AttachmentManager used to read attachments.
    std::shared_ptr<avsCommon::avs::attachment::AttachmentManagerInterface> m_attachmentManager;

    /// The set of @c SpeechSynthesizerObserver instances to notify of state changes.
    std::unordered_set<std::shared_ptr<SpeechSynthesizerObserver>> m_observers;

    /**
     * The current state of the @c SpeechSynthesizer. @c m_mutex must be acquired before reading or writing the
     * @c m_currentState.
     */
    SpeechSynthesizerObserver::SpeechSynthesizerState m_currentState;

    /**
     * The state the @c SpeechSynthesizer must transition to. @c m_mutex must be acquired before reading or writing
     * the @c m_desiredState.
     */
    SpeechSynthesizerObserver::SpeechSynthesizerState m_desiredState;

    /// The current focus acquired by the @c SpeechSynthesizer.
    avsCommon::avs::FocusState m_currentFocus;

    /// @c SpeakDirectiveInfo instance for the @c AVSDirective currently being handled.
    std::shared_ptr<SpeakDirectiveInfo> m_currentInfo;

    /// Mutex to serialize access to m_currentState, m_desiredState, and m_waitOnStateChange.
    std::mutex m_mutex;

    /// Condition variable to wake @c onFocusChanged() once the state transition to desired state is complete.
    std::condition_variable m_waitOnStateChange;

    /// Map of message Id to @c SpeakDirectiveInfo.
    std::unordered_map<std::string, std::shared_ptr<SpeakDirectiveInfo>> m_speakDirectiveInfoMap;

    /**
     * Mutex to protect @c messageId to @c SpeakDirectiveInfo mapping. This mutex must be acquired before accessing
     * or modifying the m_speakDirectiveInfoMap
     */
    std::mutex m_speakDirectiveInfoMutex;

    /// Queue which holds the directives to be processed.
    std::deque<std::shared_ptr<SpeakDirectiveInfo>> m_speakInfoQueue;

    /// Serializes access to @c m_speakInfoQueue
    std::mutex m_speakInfoQueueMutex;

    /**
     * @c Executor which queues up operations from asynchronous API calls.
     *
     * @note This declaration needs to come *after* any variables used by the executor thread so that the thread shuts
     *     down before the variables are destroyed.
     */
    avsCommon::utils::threading::Executor m_executor;
};

}  // namespace speechSynthesizer
}  // namespace capabilityAgents
}  // namespace alexaClientSDK

#endif  // ALEXA_CLIENT_SDK_CAPABILITY_AGENTS_SPEECH_SYNTHESIZER_INCLUDE_SPEECH_SYNTHESIZER_SPEECH_SYNTHESIZER_H_
