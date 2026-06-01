//
//	License type: BSD 3-Clause License
//	License copy: https://github.com/Telecominfraproject/wlan-cloud-ucentralgw/blob/master/LICENSE
//
//	Created by Stephane Bourque on 2021-03-04.
//	Arilia Wireless Inc.
//

#include "RESTAPI_inventory_list_handler.h"
#include "RESTAPI/RESTAPI_db_helpers.h"
#include "RESTAPI/RESTAPI_rbac_helpers.h"
#include "StorageService.h"

namespace OpenWifi {
	void RESTAPI_inventory_list_handler::SendList(const ProvObjects::InventoryTagVec &Tags,
												  bool SerialOnly) {
		Poco::JSON::Array Array;
		for (const auto &i : Tags) {
			if (SerialOnly) {
				Array.add(i.serialNumber);
			} else {
				Poco::JSON::Object O;
				i.to_json(O);
				if (QB_.AdditionalInfo)
					AddExtendedInfo(i, O);
				Array.add(O);
			}
		}
		Poco::JSON::Object Answer;
		if (SerialOnly)
			Answer.set("serialNumbers", Array);
		else
			Answer.set("taglist", Array);
		ReturnObject(Answer);
	}

	void RESTAPI_inventory_list_handler::DoGet() {

		if (GetBoolParameter("orderSpec")) {
			return ReturnFieldList(DB_, *this);
		}

		bool SerialOnly = GetBoolParameter("serialOnly");
		auto filterByScope = [&](ProvObjects::InventoryTagVec &tags) {
			if (RBAC::IsRootUser(*this)) {
				return;
			}
			ProvObjects::InventoryTagVec filtered;
			for (const auto &t : tags) {
				if (RBAC::IsScopeAllowed(*this, RBAC::TargetScope{t.entity, t.venue})) {
					filtered.push_back(t);
				}
			}
			tags = std::move(filtered);
		};

		std::string UUID;
		std::string Arg, Arg2;

		std::string OrderBy{" ORDER BY serialNumber ASC "};
		if (HasParameter("orderBy", Arg)) {
			if (!DB_.PrepareOrderBy(Arg, OrderBy)) {
				return BadRequest(RESTAPI::Errors::InvalidLOrderBy);
			}
		}

		if (!QB_.Select.empty()) {
			return ReturnRecordList<decltype(DB_)>("taglist", DB_, *this);
		} else if (HasParameter("entity", UUID)) {
			if (QB_.CountOnly) {
				ProvObjects::InventoryTagVec Tags;
				DB_.GetRecords(QB_.Offset, QB_.Limit, Tags, DB_.OP("entity", ORM::EQ, UUID), OrderBy);
				filterByScope(Tags);
				return ReturnCountOnly(Tags.size());
			}
			ProvObjects::InventoryTagVec Tags;
			DB_.GetRecords(QB_.Offset, QB_.Limit, Tags, DB_.OP("entity", ORM::EQ, UUID), OrderBy);
			filterByScope(Tags);
			return SendList(Tags, SerialOnly);
		} else if (HasParameter("venue", UUID)) {
			if (QB_.CountOnly) {
				ProvObjects::InventoryTagVec Tags;
				DB_.GetRecords(QB_.Offset, QB_.Limit, Tags, DB_.OP("venue", ORM::EQ, UUID), OrderBy);
				filterByScope(Tags);
				return ReturnCountOnly(Tags.size());
			}
			ProvObjects::InventoryTagVec Tags;
			DB_.GetRecords(QB_.Offset, QB_.Limit, Tags, DB_.OP("venue", ORM::EQ, UUID), OrderBy);
			filterByScope(Tags);
			return SendList(Tags, SerialOnly);
		} else if (GetBoolParameter("subscribersOnly") && GetBoolParameter("unassigned")) {
			if (QB_.CountOnly) {
				ProvObjects::InventoryTagVec Tags;
				DB_.GetRecords(QB_.Offset, QB_.Limit, Tags, " devClass='subscriber' and subscriber='' ",
							   OrderBy);
				filterByScope(Tags);
				return ReturnCountOnly(Tags.size());
			}
			ProvObjects::InventoryTagVec Tags;
			DB_.GetRecords(QB_.Offset, QB_.Limit, Tags, " devClass='subscriber' and subscriber='' ",
						   OrderBy);
			filterByScope(Tags);
			if (QB_.CountOnly) {
				return ReturnCountOnly(Tags.size());
			}
			return SendList(Tags, SerialOnly);
		} else if (GetBoolParameter("subscribersOnly")) {
			if (QB_.CountOnly) {
				ProvObjects::InventoryTagVec Tags;
				DB_.GetRecords(QB_.Offset, QB_.Limit, Tags,
							   " devClass='subscriber' and subscriber!='' ", OrderBy);
				filterByScope(Tags);
				return ReturnCountOnly(Tags.size());
			}
			ProvObjects::InventoryTagVec Tags;
			DB_.GetRecords(QB_.Offset, QB_.Limit, Tags,
						   " devClass='subscriber' and subscriber!='' ", OrderBy);
			filterByScope(Tags);
			return SendList(Tags, SerialOnly);
		} else if (GetBoolParameter("unassigned")) {
			if (QB_.CountOnly) {
				ProvObjects::InventoryTagVec Tags;
				std::string Empty;
				DB_.GetRecords(QB_.Offset, QB_.Limit, Tags,
							   InventoryDB::OP(DB_.OP("venue", ORM::EQ, Empty), ORM::AND,
											   DB_.OP("entity", ORM::EQ, Empty)),
							   OrderBy);
				filterByScope(Tags);
				return ReturnCountOnly(Tags.size());
			}
			ProvObjects::InventoryTagVec Tags;
			std::string Empty;
			DB_.GetRecords(QB_.Offset, QB_.Limit, Tags,
						   InventoryDB::OP(DB_.OP("venue", ORM::EQ, Empty), ORM::AND,
										   DB_.OP("entity", ORM::EQ, Empty)),
						   OrderBy);
			filterByScope(Tags);
			return SendList(Tags, SerialOnly);
		} else if (HasParameter("subscriber", Arg) && !Arg.empty()) {
			// looking for device(s) for a specific subscriber...
			ProvObjects::InventoryTagVec Tags;
			DB_.GetRecords(0, 100, Tags, " subscriber='" + ORM::Escape(Arg) + "'");
			filterByScope(Tags);
			if (SerialOnly) {
				std::vector<std::string> SerialNumbers;
				std::transform(cbegin(Tags), cend(Tags), std::back_inserter(SerialNumbers),
							   [](const auto &T) { return T.serialNumber; });
				return ReturnObject("serialNumbers", SerialNumbers);
			} else {
				return MakeJSONObjectArray("taglist", Tags, *this);
			}
		} else if (QB_.CountOnly) {
			ProvObjects::InventoryTagVec Tags;
			DB_.GetRecords(QB_.Offset, QB_.Limit, Tags, "", OrderBy);
			filterByScope(Tags);
			return ReturnCountOnly(Tags.size());
		} else if (GetBoolParameter("rrmOnly")) {
			Types::UUIDvec_t DeviceList;
			DB_.GetRRMDeviceList(DeviceList);
			if (!RBAC::IsRootUser(*this)) {
				Types::UUIDvec_t filteredSerials;
				for (const auto &serial : DeviceList) {
					ProvObjects::InventoryTag tag;
					if (DB_.GetRecord("serialNumber", serial, tag) &&
						RBAC::IsScopeAllowed(*this, RBAC::TargetScope{tag.entity, tag.venue})) {
						filteredSerials.push_back(serial);
					}
				}
				return ReturnObject("serialNumbers", filteredSerials);
			}
			return ReturnObject("serialNumbers", DeviceList);
		} else {
			ProvObjects::InventoryTagVec Tags;
			DB_.GetRecords(QB_.Offset, QB_.Limit, Tags, "", OrderBy);
            filterByScope(Tags);
            return SendList(Tags, SerialOnly);

//			return MakeJSONObjectArray("taglist", Tags, *this);
		}
	}
} // namespace OpenWifi
