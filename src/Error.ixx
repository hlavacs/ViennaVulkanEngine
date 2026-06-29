export module VEEngine.Error;
import std;

/**
	* @file
	* @brief Public error contract shared by facade and engine implementations.
	*/
export namespace vve {

	namespace detail {

		template <typename TKey, typename TValue>
		[[nodiscard]] auto mapValueOr(const std::map<TKey, TValue> &values, TKey key, TValue fallback)	-> TValue{
			if (const auto match = values.find(key); match != values.end()) { return match->second; }
			return fallback;
		}

	} // namespace detail

	enum class Error {
		ok,																///< Operation completed without error.
		not_initialized,												///< Required subsystem was not initialized.
		already_initialized,											///< Subsystem was initialized more than once.
		invalid_argument,												///< Caller supplied invalid input.
		file_not_found,													///< Requested file path could not be resolved.
		io_error,														///< Input or output operation failed.
		unsupported_version,											///< Data version is not supported.
		internal_error,													///< Engine detected an internal failure.
		invalid_handle,													///< Handle does not refer to a live object.
		duplicate_object,												///< Object already exists in the target container.
		missing_object,													///< Object was required but not found.
		duplicate_component,											///< Component already exists on the entity.
		missing_component,												///< Component was required but not found.
		platform_error,													///< Platform API returned an error.
		asset_import_failed,											///< Asset import pipeline failed.
		cycle_detected													///< Graph or hierarchy contains a cycle.
	};

	[[nodiscard]] inline auto errorName(Error error)	-> std::string_view{
		static const std::map<Error, std::string_view> names{
				{Error::ok, "ok"},
				{Error::not_initialized, "not_initialized"},
				{Error::already_initialized, "already_initialized"},
				{Error::invalid_argument, "invalid_argument"},
				{Error::file_not_found, "file_not_found"},
				{Error::io_error, "io_error"},
				{Error::unsupported_version, "unsupported_version"},
				{Error::internal_error, "internal_error"},
				{Error::invalid_handle, "invalid_handle"},
				{Error::duplicate_object, "duplicate_object"},
				{Error::missing_object, "missing_object"},
				{Error::duplicate_component, "duplicate_component"},
				{Error::missing_component, "missing_component"},
				{Error::platform_error, "platform_error"},
				{Error::asset_import_failed, "asset_import_failed"},
				{Error::cycle_detected, "cycle_detected"},
		};
		return detail::mapValueOr(names, error, std::string_view{"unknown_error"});
	}

} // namespace vve
