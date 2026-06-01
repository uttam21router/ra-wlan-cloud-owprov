//
//	License type: BSD 3-Clause License
//	License copy: https://github.com/Telecominfraproject/wlan-cloud-ucentralgw/blob/master/LICENSE
//
//	Created by Stephane Bourque on 2021-03-04.
//	Arilia Wireless Inc.
//

#include "RESTAPI_entity_list_handler.h"
#include "RESTAPI_db_helpers.h"
#include "RESTAPI_rbac_helpers.h"
#include "StorageService.h"

namespace OpenWifi {
	namespace {
		bool BuildFilteredTree(RESTAPIHandler &handler, EntityDB &entityDB, const std::string &nodeId,
							   Poco::JSON::Object &nodeOut) {
			ProvObjects::Entity entity;
			if (!entityDB.GetRecord("id", nodeId, entity)) {
				return false;
			}

			const bool selfVisible = RBAC::IsEntityVisible(handler, entity.info.id);

			Poco::JSON::Array children;
			bool hasVisibleChild = false;
			for (const auto &childId : entity.children) {
				Poco::JSON::Object childNode;
				if (BuildFilteredTree(handler, entityDB, childId, childNode)) {
					children.add(childNode);
					hasVisibleChild = true;
				}
			}

			Poco::JSON::Array venues;
			for (const auto &venueId : entity.venues) {
				if (!RBAC::IsVenueVisible(handler, venueId)) {
					continue;
				}
				Poco::JSON::Object venueNode;
				entityDB.AddVenues(venueNode, venueId);
				venues.add(venueNode);
			}

			if (!selfVisible && !hasVisibleChild && venues.size() == 0) {
				return false;
			}

			nodeOut.set("type", "entity");
			nodeOut.set("name", entity.info.name);
			nodeOut.set("uuid", entity.info.id);
			nodeOut.set("children", children);
			nodeOut.set("venues", venues);
			return true;
		}
	} // namespace

	void RESTAPI_entity_list_handler::DoGet() {
		if (!QB_.Select.empty()) {
			if (RBAC::IsRootUser(*this)) {
				return ReturnRecordList<decltype(DB_), ProvObjects::Entity>("entities", DB_, *this);
			}
			ProvObjects::EntityVec filtered;
			for (const auto &id : SelectedRecords()) {
				ProvObjects::Entity e;
				if (DB_.GetRecord("id", id, e) &&
					RBAC::IsEntityVisible(*this, e.info.id)) {
					filtered.push_back(e);
				}
			}
			return MakeJSONObjectArray("entities", filtered, *this);
		} else if (QB_.CountOnly) {
			if (RBAC::IsRootUser(*this)) {
				auto C = DB_.Count();
				return ReturnCountOnly(C);
			}
			EntityDB::RecordVec entities;
			DB_.GetRecords(QB_.Offset, QB_.Limit, entities);
			std::size_t count = 0;
			for (const auto &e : entities) {
				if (RBAC::IsEntityVisible(*this, e.info.id)) {
					++count;
				}
			}
			return ReturnCountOnly(count);
		} else if (GetBoolParameter("getTree", false)) {
			if (RBAC::IsRootUser(*this)) {
				Poco::JSON::Object FullTree;
				DB_.BuildTree(FullTree);
				return ReturnObject(FullTree);
			}

			Poco::JSON::Object filteredTree;
			if (BuildFilteredTree(*this, DB_, EntityDB::RootUUID(), filteredTree)) {
				return ReturnObject(filteredTree);
			}
			Poco::JSON::Object emptyTree;
			emptyTree.set("type", "entity");
			emptyTree.set("name", "root");
			emptyTree.set("uuid", EntityDB::RootUUID());
			emptyTree.set("children", Poco::JSON::Array());
			emptyTree.set("venues", Poco::JSON::Array());
			return ReturnObject(emptyTree);
		} else {
			EntityDB::RecordVec Entities;
			DB_.GetRecords(QB_.Offset, QB_.Limit, Entities);
			if (RBAC::IsRootUser(*this)) {
				return MakeJSONObjectArray("entities", Entities, *this);
			}
			EntityDB::RecordVec filtered;
			for (const auto &e : Entities) {
				if (RBAC::IsEntityVisible(*this, e.info.id)) {
					filtered.push_back(e);
				}
			}
			return MakeJSONObjectArray("entities", filtered, *this);
		}
	}

	void RESTAPI_entity_list_handler::DoPost() {
		if (GetBoolParameter("setTree", false)) {
			const auto &FullTree = ParsedBody_;
			DB_.ImportTree(FullTree);
			return OK();
		}
		BadRequest(RESTAPI::Errors::MissingOrInvalidParameters);
	}
} // namespace OpenWifi
