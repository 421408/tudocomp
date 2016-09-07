/*
    This file exists merely to provide Doxygen documentation for the namespaces themselves.

    In the future, this may be used as a shortcut to include everything.
*/

/// \brief Contains the text compression and encoding framework.
///
/// This is the framework's central namespace in which text compression and
/// coding algorithms are contained, as well as utilities needed for text
/// compression and coding (e.g. I/O). Families of compressors and encoders
/// or utility groups are contained in the respective sub-namespaces. The
/// namespace \c tudocomp itself contains types important for all of the
/// framework and its communication.
namespace tudocomp {
}

/// \brief Contains the I/O abstraction classes for reading and writing text.
///
/// All I/O done by compressors and encoders is routed through the \ref Input
/// and \ref Output abstractions over the underlying file, memory or stream
/// I/O.
///
/// \sa
/// \ref Input for the input interface and \ref Output for the output
/// interface.
namespace tudocomp::io {
}

/// \brief Contains compressors and encoders that work with Lempel-Ziv-78-like
/// dictionaries.
///
/// The LZ78 family works with bottom-up dictionaries containing indexed
/// entries to achieve compression. Each entry points to a \e prefix (another
/// dictionary entry) and a follow-up symbol.
namespace tudocomp::lz78 {
}

/// \brief Contains compressors and encoders that work with
/// Lempel-Ziv-Storer-Szymansky-like factors.
///
/// The LZSS family works with factors representing references to positions
/// within the original text that replace parts of the same text, effectively
/// using the input text itself as a dictionary. They consist of a \e source
/// text position and a \e length.
namespace tudocomp::lzss {
}

/// \brief Contains compressors and encoders that work with
/// Lempel-Ziv-Welch-like dictionaries.
///
/// The LZW family works with bottom-up dictionaries containing indexed entries
/// to achieve compression. Other than \ref lz78, the dictionary entries do not
/// explicitly store the follow-up symbol. Instead, they are re-generated on
/// the fly by the decoder.
namespace tudocomp::lzw {
}