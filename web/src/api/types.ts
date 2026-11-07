// Kurze Namen für die Typen, die openapi-typescript aus docs/api.yaml erzeugt
// (API-01). Kein Typ der Oberfläche beschreibt die API von Hand.
import type { components } from "./schema";

type Schemas = components["schemas"];

export type State = Schemas["State"];
export type Role = Schemas["Role"];
export type RoleStatus = Schemas["RoleStatus"];
export type Fault = Schemas["Fault"];
export type ReadStatus = Schemas["ReadStatus"];
export type CommandType = Schemas["CommandType"];
export type CommandResult = Schemas["CommandResult"];
export type Limits = Schemas["Limits"];
export type RoleReading = Schemas["RoleReading"];
export type Channel = Schemas["Channel"];
export type CommandRecord = Schemas["CommandRecord"];
export type StateSnapshot = Schemas["StateSnapshot"];
export type CommandResponse = Schemas["CommandResponse"];
export type SetpointRequest = Schemas["SetpointRequest"];
export type RoleRequest = Schemas["RoleRequest"];
export type OffsetRequest = Schemas["OffsetRequest"];
export type RequestError = Schemas["RequestError"];
export type RequestErrorCode = RequestError["error"];
