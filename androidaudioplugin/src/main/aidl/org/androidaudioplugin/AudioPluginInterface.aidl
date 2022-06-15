package org.androidaudioplugin;
import org.androidaudioplugin.AudioPluginInterfaceCallback;

interface AudioPluginInterface {
    const int AAP_BINDER_ERROR_UNEXPECTED_INSTANCE_ID = 1;
    const int AAP_BINDER_ERROR_CREATE_INSTANCE_FAILED = 2;
    const int AAP_BINDER_ERROR_SHARED_MEMORY_EXTENSION = 10;
    const int AAP_BINDER_ERROR_MMAP_FAILED = 11;
    const int AAP_BINDER_ERROR_MMAP_NULL_RETURN = 12;
    const int AAP_BINDER_ERROR_INVALID_SHARED_MEMORY_FD = 20;
    const int AAP_BINDER_ERROR_CALLBACK_ALREADY_SET = 30;

    // ServiceConnection operations
	void setCallback(in AudioPluginInterfaceCallback callback);

    // Instance operations
	int beginCreate(String pluginId, int sampleRate);
	void addExtension(int instanceID, String uri, in ParcelFileDescriptor sharedMemoryFD, int size);
	void endCreate(int instanceID);

	boolean isPluginAlive(int instanceID);

    // FIXME: remove them from AIDL. They should be invoked only via AAPXS, hopefully before 0.7.2 release.
	int getStateSize(int instanceID);
	void getState(int instanceID, in ParcelFileDescriptor sharedMemoryFD);
	void setState(int instanceID, in ParcelFileDescriptor sharedMemoryFD, int size);

	void extension(int instanceID, String uri, int size);

	void prepare(int instanceID, int frameCount, int portCount);
	void prepareMemory(int instanceID, int shmFDIndex, in ParcelFileDescriptor sharedMemoryFD);
	void activate(int instanceID);
	void process(int instanceID, int timeoutInNanoseconds);
	void deactivate(int instanceID);
	
	void destroy(int instanceID);
}

