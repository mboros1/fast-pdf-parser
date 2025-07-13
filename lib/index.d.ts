export interface ChunkOptions {
    /** Maximum tokens per chunk (default: 512) */
    maxTokens?: number;
    /** Minimum tokens per chunk (default: 150) */
    minTokens?: number;
    /** Number of overlapping tokens between chunks (default: 0) */
    overlapTokens?: number;
    /** Number of threads to use (default: hardware concurrency) */
    threadCount?: number;
}

export interface ChunkResult {
    /** The text content of the chunk */
    text: string;
    /** Number of tokens in the chunk */
    tokenCount: number;
    /** Starting page number (0-based) */
    startPage: number;
    /** Ending page number (0-based) */
    endPage: number;
    /** Whether the chunk contains a major heading */
    hasMajorHeading: boolean;
    /** Minimum heading level in the chunk (lower = more important) */
    minHeadingLevel: number;
}

export interface ChunkingResult {
    /** Array of extracted chunks */
    chunks: ChunkResult[];
    /** Total number of pages processed */
    totalPages: number;
    /** Total number of chunks created */
    totalChunks: number;
    /** Processing time in milliseconds */
    processingTimeMs: number;
}

export class HierarchicalChunker {
    /**
     * Create a new HierarchicalChunker instance
     * @param options - Configuration options for chunking
     */
    constructor(options?: ChunkOptions);
    
    /**
     * Chunk a PDF file into semantic text chunks
     * @param pdfPath - Path to the PDF file
     * @param pageLimit - Optional limit on number of pages to process
     * @returns Chunking results
     * @throws Error if PDF cannot be processed
     */
    chunkFile(pdfPath: string, pageLimit?: number): ChunkingResult;
    
    /**
     * Get current chunking options
     * @returns Current configuration options
     */
    getOptions(): ChunkOptions;
    
    /**
     * Update chunking options
     * @param options - New configuration options (partial update supported)
     */
    setOptions(options: ChunkOptions): void;
}

/**
 * Convenience function for one-shot PDF chunking
 * @param pdfPath - Path to the PDF file
 * @param options - Configuration options including optional pageLimit
 * @returns Chunking results
 * @throws Error if PDF cannot be processed
 */
export function chunkPdf(pdfPath: string, options?: ChunkOptions & { pageLimit?: number }): ChunkingResult;