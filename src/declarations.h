#pragma once

namespace relevanced {

namespace util {
class ClockIf;
class Clock;
class Sha1HasherIf;
class Sha1Hasher;
} // util

namespace centroid_update_worker {
class CentroidUpdateWorkerIf;
class CentroidUpdateWorker;
class CentroidUpdaterIf;
class CentroidUpdater;
class CentroidUpdaterFactoryIf;
class CentroidUpdaterFactory;
} // centroid_update_worker

namespace document_processing_worker {
class DocumentProcessingWorkerIf;
class DocumentProcessorIf;
class DocumentProcessor;
} // document_processing_worker

namespace stemmer {
class StemmerIf;
class StemmerManagerIf;
class ThreadSafeStemmerManager;
class PorterStemmer;
} // stemmmer

namespace stopwords {
class StopwordFilterIf;
class StopwordFilter;
} // stopwords

namespace similarity_score_worker {
class SimilarityScoreWorkerIf;
} // similarity_score_worker

namespace persistence {
class PersistenceIf;
class Persistence;
class SyncPersistenceIf;
class SyncPersistence;
class CentroidMetadataDbIf;
class CentroidMetadataDb;
class RockHandleIf;
class RockHandle;
} // persistence

namespace server {
class RelevanceServerIf;
class ThriftRelevanceServer;
} // server

namespace models {
class Document;
class Centroid;
class ProcessedDocument;
class WordVector;
} // models

} // relevanced