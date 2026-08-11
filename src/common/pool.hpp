#pragma once

#include "types.hpp"

#include <cassert>
#include <tuple>
#include <utility>
#include <vector>

template <typename... Ts>
class Pool {
	std::tuple<std::vector<Ts>...> m_data;

	std::vector<ID> m_dense_to_Id;
	std::vector<ID> m_slots;
	std::vector<ID> m_free_ids;

	template <typename T>
	std::vector<T>& data() {
		return std::get<std::vector<T>>(m_data);
	}

	template <typename T>
	const std::vector<T>& data() const {
		return std::get<std::vector<T>>(m_data);
	}

	template <typename... Args>
	void emplace_components(Args&&... args) {
		static_assert(sizeof...(Args) == sizeof...(Ts));

		(data<Ts>().emplace_back(std::forward<Args>(args)), ...);
	}

	void pop_back_all() {
		(data<Ts>().pop_back(), ...);
	}

	void move_dense(ID dst, ID src) {
		((data<Ts>()[dst] = std::move(data<Ts>()[src])), ...);
	}

  public:
	template <typename... Args>
	ID emplace(Args&&... args) {
		static_assert(sizeof...(Args) == sizeof...(Ts));

		ID id;

		if (!m_free_ids.empty()) {
			id = m_free_ids.back();
			m_free_ids.pop_back();
		} else {
			id = static_cast<ID>(m_slots.size());
			m_slots.push_back(INVALID_ID);
		}

		const ID dense_index = static_cast<ID>(m_dense_to_Id.size());

		m_slots[id] = dense_index;

		emplace_components(std::forward<Args>(args)...);
		m_dense_to_Id.push_back(id);

		return id;
	}

	void erase(ID id) {
		assert(contains(id));

		const ID dense_index = m_slots[id];
		const ID last_index = static_cast<ID>(m_dense_to_Id.size() - 1);

		if (dense_index != last_index) {
			move_dense(dense_index, last_index);

			const ID moved_id = m_dense_to_Id[last_index];

			m_dense_to_Id[dense_index] = moved_id;
			m_slots[moved_id] = dense_index;
		}

		pop_back_all();
		m_dense_to_Id.pop_back();

		m_slots[id] = INVALID_ID;
		m_free_ids.push_back(id);
	}

	[[nodiscard]]
	bool contains(ID id) const {
		return id < m_slots.size() && m_slots[id] != INVALID_ID;
	}

	template <typename T>
	T& get(ID id) {
		assert(contains(id));
		return data<T>()[m_slots[id]];
	}

	template <typename T>
	const T& get(ID id) const {
		assert(contains(id));
		return data<T>()[m_slots[id]];
	}

	template <typename T>
	std::vector<T>& dense() {
		return data<T>();
	}

	template <typename T>
	const std::vector<T>& dense() const {
		return data<T>();
	}

	[[nodiscard]]
	ID dense_index(ID id) const {
		assert(contains(id));
		return m_slots[id];
	}

	[[nodiscard]]
	ID id_at(ID dense_index) const {
		assert(dense_index < m_dense_to_Id.size());
		return m_dense_to_Id[dense_index];
	}

	[[nodiscard]]
	size_t size() const {
		return m_dense_to_Id.size();
	}

	[[nodiscard]]
	bool empty() const {
		return m_dense_to_Id.empty();
	}

	void reserve(size_t count) {
		(data<Ts>().reserve(count), ...);

		m_dense_to_Id.reserve(count);
		m_slots.reserve(count);
	}

	void clear() {
		(data<Ts>().clear(), ...);

		m_dense_to_Id.clear();
		m_slots.clear();
		m_free_ids.clear();
	}
};